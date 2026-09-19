#include "parse.h"

#include <tree_sitter/api.h>

#include <cctype>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "prepass.h"

extern "C" const TSLanguage* tree_sitter_cpp(void);

namespace xrefl {
namespace {

bool is_type(TSNode n, const char* type) { return std::strcmp(ts_node_type(n), type) == 0; }

// Collapses whitespace and drops the space cutting the name out leaves before
// an array bound, so `int arr[4]` spells as `int[4]`.
std::string normalize_spelling(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    bool pending_space = false;
    for (char c : in) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
            pending_space = !out.empty();
            continue;
        }
        if (pending_space && c != '[') out += ' ';
        pending_space = false;
        out += c;
    }
    return out;
}

class UnitBuilder {
public:
    UnitBuilder(const Source& source, const Prepass& prepass, Unit& unit, DiagSink& diags)
        : src_(source), prepass_(prepass), unit_(unit), diags_(diags) {
        for (size_t i = 0; i < prepass_.sites.size(); ++i) {
            claims_[prepass_.sites[i].attaches_to].push_back(i);
        }
        attached_.assign(prepass_.sites.size(), false);
    }

    void walk_scope(TSNode node);
    void report_unattached(TSNode root);
    std::string absorbing_token(TSNode root, uint32_t at, uint32_t annotation_begin) const;

private:
    std::string text(TSNode n) const {
        return std::string(src_.slice(ts_node_start_byte(n), ts_node_end_byte(n)));
    }

    Location loc(TSNode n) const { return src_.locate(ts_node_start_byte(n)); }

    std::vector<AnnotationRef> take_annotations(TSNode node) {
        std::vector<AnnotationRef> out;
        auto it = claims_.find(ts_node_start_byte(node));
        if (it == claims_.end()) return out;
        for (size_t index : it->second) {
            const AnnotationSite& site = prepass_.sites[index];
            attached_[index] = true;
            out.push_back({site.name, site.args, src_.locate(site.begin)});
        }
        return out;
    }

    bool has_annotations(TSNode node) const {
        return claims_.count(ts_node_start_byte(node)) != 0;
    }

    std::string scope_name(const std::string& name) const {
        if (scope_.empty()) return name;
        return scope_ + "::" + name;
    }

    void collect_include(TSNode node);
    // Visits a record or enum defined in the `type` position of a declaration.
    // Returns the specifier node if there was one.
    TSNode visit_inline_type(TSNode decl, const std::string& enclosing);
    void visit_record(TSNode node);
    void visit_enum(TSNode node);
    void visit_declaration(TSNode node);
    void visit_template(TSNode node);

    void collect_bases(TSNode clause, Struct& out);
    void collect_members(TSNode body, Struct& out, const std::string& default_access);
    void collect_member(TSNode node, Struct& out, const std::string& access);
    void collect_params(TSNode parameter_list, std::vector<Param>& out);

    // Joins the shared prefix (qualifiers and type name) to one declarator with
    // the declared name cut out. Per declarator, so `int a, *b, c[3]` gives
    // three types.
    std::string spell_type(uint32_t prefix_begin, uint32_t prefix_end, TSNode declarator,
                           TSNode identifier) const;
    // A declaration can introduce several names at once.
    std::vector<TSNode> declarators_of(TSNode decl) const;
    TSNode type_start_node(TSNode decl) const;
    bool is_mutable_qualifier(TSNode node) const;
    static TSNode innermost_declarator_id(TSNode declarator);
    static TSNode find_function_declarator(TSNode declarator);

    const Source& src_;
    const Prepass& prepass_;
    Unit& unit_;
    DiagSink& diags_;
    std::unordered_map<uint32_t, std::vector<size_t>> claims_;
    std::vector<bool> attached_;
    std::string scope_;
};

// The grammar calls `mutable` a type qualifier, but it belongs to the field,
// not the type.
bool UnitBuilder::is_mutable_qualifier(TSNode node) const {
    return is_type(node, "type_qualifier") && text(node) == "mutable";
}

// Skips specifiers that belong to the declaration rather than the type.
TSNode UnitBuilder::type_start_node(TSNode decl) const {
    uint32_t count = ts_node_child_count(decl);
    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_child(decl, i);
        const char* t = ts_node_type(child);
        if (std::strcmp(t, "storage_class_specifier") == 0 || std::strcmp(t, "virtual") == 0 ||
            std::strcmp(t, "explicit_function_specifier") == 0 ||
            std::strcmp(t, "attribute_declaration") == 0 ||
            std::strcmp(t, "attribute_specifier") == 0 ||
            std::strcmp(t, "ms_declspec_modifier") == 0 || is_mutable_qualifier(child)) {
            continue;
        }
        return child;
    }
    return ts_node_child(decl, 0);
}

bool is_declared_name(TSNode n) {
    const char* t = ts_node_type(n);
    return std::strcmp(t, "field_identifier") == 0 || std::strcmp(t, "identifier") == 0 ||
           std::strcmp(t, "qualified_identifier") == 0 || std::strcmp(t, "operator_name") == 0 ||
           std::strcmp(t, "destructor_name") == 0;
}

// Walks down to the declared name. Reference and parenthesised declarators
// hold their inner declarator as an unnamed child, hence the fallback scan.
TSNode UnitBuilder::innermost_declarator_id(TSNode declarator) {
    TSNode current = declarator;
    for (int guard = 0; guard < 64 && !ts_node_is_null(current); ++guard) {
        if (is_declared_name(current)) return current;

        TSNode next = ts_node_child_by_field_name(current, "declarator", 10);
        if (ts_node_is_null(next)) {
            uint32_t count = ts_node_named_child_count(current);
            for (uint32_t i = 0; i < count; ++i) {
                TSNode child = ts_node_named_child(current, i);
                const char* t = ts_node_type(child);
                if (is_declared_name(child) || std::strstr(t, "declarator") != nullptr) {
                    next = child;
                    break;
                }
            }
        }
        if (ts_node_is_null(next)) return current;
        current = next;
    }
    return declarator;
}

// A reference declarator holds its inner declarator as an unnamed child, so
// `T& f()` is only found by scanning past the field lookup.
TSNode UnitBuilder::find_function_declarator(TSNode declarator) {
    TSNode current = declarator;
    for (int guard = 0; guard < 64 && !ts_node_is_null(current); ++guard) {
        if (is_type(current, "function_declarator")) return current;
        TSNode next = ts_node_child_by_field_name(current, "declarator", 10);
        if (ts_node_is_null(next)) {
            uint32_t count = ts_node_named_child_count(current);
            for (uint32_t i = 0; i < count; ++i) {
                TSNode child = ts_node_named_child(current, i);
                if (std::strstr(ts_node_type(child), "declarator") != nullptr) {
                    next = child;
                    break;
                }
            }
        }
        if (ts_node_is_null(next)) break;
        current = next;
    }
    return TSNode{};
}

std::vector<TSNode> UnitBuilder::declarators_of(TSNode decl) const {
    std::vector<TSNode> out;
    uint32_t count = ts_node_child_count(decl);
    for (uint32_t i = 0; i < count; ++i) {
        const char* field = ts_node_field_name_for_child(decl, i);
        if (field && std::strcmp(field, "declarator") == 0) out.push_back(ts_node_child(decl, i));
    }
    return out;
}

std::string UnitBuilder::spell_type(uint32_t prefix_begin, uint32_t prefix_end, TSNode declarator,
                                    TSNode identifier) const {
    std::string spelled(src_.slice(prefix_begin, prefix_end));

    uint32_t a = ts_node_start_byte(declarator);
    uint32_t b = ts_node_end_byte(declarator);
    if (ts_node_is_null(identifier)) {
        spelled += std::string(src_.slice(a, b));
    } else {
        spelled += std::string(src_.slice(a, ts_node_start_byte(identifier)));
        spelled += std::string(src_.slice(ts_node_end_byte(identifier), b));
    }
    return normalize_spelling(spelled);
}

void UnitBuilder::collect_include(TSNode node) {
    TSNode path = ts_node_child_by_field_name(node, "path", 4);
    if (ts_node_is_null(path)) return;
    std::string raw = text(path);
    if (raw.size() >= 2) {
        char open = raw.front();
        char close = raw.back();
        if ((open == '"' && close == '"') || (open == '<' && close == '>')) {
            raw = raw.substr(1, raw.size() - 2);
        }
    }
    unit_.includes.push_back(raw);
}

void UnitBuilder::collect_bases(TSNode clause, Struct& out) {
    uint32_t count = ts_node_child_count(clause);
    std::string access;
    bool is_virtual = false;
    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_child(clause, i);
        const char* t = ts_node_type(child);
        if (std::strcmp(t, ":") == 0) continue;
        if (std::strcmp(t, ",") == 0) {
            access.clear();
            is_virtual = false;
            continue;
        }
        if (std::strcmp(t, "access_specifier") == 0) {
            access = text(child);
            continue;
        }
        if (std::strcmp(t, "virtual") == 0) {
            is_virtual = true;
            continue;
        }
        Base base;
        base.name = normalize_spelling(text(child));
        base.access = access.empty() ? (out.kind == "class" ? "private" : "public") : access;
        base.is_virtual = is_virtual;
        out.bases.push_back(std::move(base));
        access.clear();
        is_virtual = false;
    }
}

void UnitBuilder::collect_params(TSNode parameter_list, std::vector<Param>& out) {
    if (ts_node_is_null(parameter_list)) return;
    uint32_t count = ts_node_named_child_count(parameter_list);
    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_named_child(parameter_list, i);
        if (!is_type(child, "parameter_declaration") &&
            !is_type(child, "optional_parameter_declaration") &&
            !is_type(child, "variadic_parameter_declaration")) {
            continue;
        }
        Param param;
        TSNode declarator = ts_node_child_by_field_name(child, "declarator", 10);
        if (ts_node_is_null(declarator)) {
            param.type = normalize_spelling(text(child));
        } else {
            TSNode id = innermost_declarator_id(declarator);
            bool named = is_type(id, "identifier") || is_type(id, "field_identifier");
            param.name = named ? text(id) : "";
            uint32_t prefix_begin = ts_node_start_byte(type_start_node(child));
            uint32_t prefix_end = ts_node_start_byte(declarator);
            if (prefix_end < prefix_begin) prefix_end = prefix_begin;
            param.type =
                spell_type(prefix_begin, prefix_end, declarator, named ? id : TSNode{});
        }
        out.push_back(std::move(param));
    }
}

// `struct Nested { ... };` inside a class parses as a declaration whose type
// is the definition.
TSNode UnitBuilder::visit_inline_type(TSNode decl, const std::string& enclosing) {
    TSNode type_node = ts_node_child_by_field_name(decl, "type", 4);
    if (ts_node_is_null(type_node)) return TSNode{};

    bool is_record = is_type(type_node, "struct_specifier") ||
                     is_type(type_node, "class_specifier") ||
                     is_type(type_node, "union_specifier");
    bool is_enum = is_type(type_node, "enum_specifier");
    if (!is_record && !is_enum) return TSNode{};
    if (ts_node_is_null(ts_node_child_by_field_name(type_node, "body", 4))) return TSNode{};

    std::string saved = scope_;
    if (!enclosing.empty()) scope_ = scope_name(enclosing);
    if (is_enum) {
        visit_enum(type_node);
    } else {
        visit_record(type_node);
    }
    scope_ = saved;
    return type_node;
}

void UnitBuilder::collect_member(TSNode node, Struct& out, const std::string& access) {
    TSNode inline_type = visit_inline_type(node, out.name);
    std::vector<TSNode> declarators = declarators_of(node);
    if (declarators.empty()) {
        return;
    }

    uint32_t prefix_begin = ts_node_start_byte(type_start_node(node));
    uint32_t prefix_end = ts_node_start_byte(declarators.front());
    if (prefix_end < prefix_begin) prefix_end = prefix_begin;

    std::vector<AnnotationRef> annotations = take_annotations(node);

    // `struct Tagged { ... } instance;` declares a type and a field at once.
    std::string inline_type_name;
    if (!ts_node_is_null(inline_type)) {
        TSNode name = ts_node_child_by_field_name(inline_type, "name", 4);
        if (ts_node_is_null(name)) return;
        inline_type_name = text(name);
    }

    bool is_static = false;
    bool is_mutable = false;
    std::string bitfield;
    bool has_virtual = false;
    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_child(node, i);
        const char* t = ts_node_type(child);
        if (std::strcmp(t, "storage_class_specifier") == 0) {
            if (text(child) == "static") is_static = true;
        } else if (is_mutable_qualifier(child)) {
            is_mutable = true;
        } else if (std::strcmp(t, "virtual") == 0) {
            has_virtual = true;
        } else if (std::strcmp(t, "bitfield_clause") == 0) {
            std::string width = normalize_spelling(text(child));
            if (!width.empty() && width.front() == ':') width = normalize_spelling(width.substr(1));
            bitfield = width;
        }
    }
    TSNode default_value = ts_node_child_by_field_name(node, "default_value", 13);
    bool is_pure = !ts_node_is_null(default_value) && text(default_value) == "0";

    for (TSNode declarator : declarators) {
        TSNode func = find_function_declarator(declarator);
        TSNode id = innermost_declarator_id(declarator);

        // `void (*cb)(int)` also has a function_declarator on the outside. Its
        // inner declarator is parenthesised, a method's is a bare name.
        bool is_method = false;
        if (!ts_node_is_null(func)) {
            TSNode inner = ts_node_child_by_field_name(func, "declarator", 10);
            is_method = !ts_node_is_null(inner) && is_declared_name(inner);
        }

        if (!is_method) {
            Field field;
            field.name = text(id);
            field.type = inline_type_name.empty()
                             ? spell_type(prefix_begin, prefix_end, declarator, id)
                             : inline_type_name;
            field.access = access;
            field.is_static = is_static;
            field.is_mutable = is_mutable;
            field.bitfield = bitfield;
            field.annotations = annotations;
            field.loc = loc(node);
            out.fields.push_back(std::move(field));
            continue;
        }

        Method method;
        method.name = text(id);
        method.access = access;
        method.is_static = is_static;
        method.is_virtual = has_virtual;
        method.is_pure = is_pure;
        method.annotations = annotations;
        method.loc = loc(node);
        collect_params(ts_node_child_by_field_name(func, "parameters", 10), method.params);

        std::string return_type(src_.slice(prefix_begin, prefix_end));
        return_type += std::string(src_.slice(ts_node_start_byte(declarator),
                                              ts_node_start_byte(id)));
        method.return_type = normalize_spelling(return_type);

        uint32_t func_children = ts_node_child_count(func);
        for (uint32_t i = 0; i < func_children; ++i) {
            TSNode child = ts_node_child(func, i);
            if (is_type(child, "type_qualifier") && text(child) == "const") method.is_const = true;
            if (is_type(child, "trailing_return_type")) {
                std::string trailing = normalize_spelling(text(child));
                if (trailing.rfind("->", 0) == 0) {
                    trailing = normalize_spelling(trailing.substr(2));
                }
                method.return_type = trailing;
            }
        }

        out.methods.push_back(std::move(method));
    }
}

void UnitBuilder::collect_members(TSNode body, Struct& out, const std::string& default_access) {
    std::string access = default_access;
    uint32_t count = ts_node_named_child_count(body);
    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_named_child(body, i);
        const char* t = ts_node_type(child);

        if (std::strcmp(t, "access_specifier") == 0) {
            access = text(child);
            continue;
        }
        if (std::strcmp(t, "field_declaration") == 0) {
            collect_member(child, out, access);
            continue;
        }
        if (std::strcmp(t, "struct_specifier") == 0 || std::strcmp(t, "class_specifier") == 0 ||
            std::strcmp(t, "union_specifier") == 0 || std::strcmp(t, "enum_specifier") == 0 ||
            std::strcmp(t, "template_declaration") == 0) {
            std::string saved = scope_;
            scope_ = scope_name(out.name);
            walk_scope(child);
            scope_ = saved;
            continue;
        }
    }
}

void UnitBuilder::visit_record(TSNode node) {
    TSNode body = ts_node_child_by_field_name(node, "body", 4);
    TSNode name = ts_node_child_by_field_name(node, "name", 4);

    if (ts_node_is_null(body)) {
        // A forward declaration. Nothing to reflect, but annotating one is a
        // mistake.
        if (has_annotations(node)) {
            std::vector<AnnotationRef> claimed = take_annotations(node);
            diags_.error("annotated-forward-declaration",
                         "annotation '" + claimed.front().name +
                             "' is on a declaration with no body",
                         "annotate the definition of the type instead", src_,
                         ts_node_start_byte(node));
        }
        return;
    }

    Struct record;
    record.ns = scope_;
    record.name = ts_node_is_null(name) ? "" : text(name);
    const char* t = ts_node_type(node);
    record.kind = (std::strcmp(t, "class_specifier") == 0)
                      ? "class"
                      : (std::strcmp(t, "union_specifier") == 0 ? "union" : "struct");
    record.annotations = take_annotations(node);
    record.loc = loc(node);

    if (record.name.empty() && !record.annotations.empty()) {
        diags_.error("annotated-anonymous-type", "annotation on a type with no name",
                     "give the type a name, or move the annotation to its members", src_,
                     ts_node_start_byte(node));
        return;
    }

    TSNode bases = ts_node_child_by_field_name(node, "base_class_clause", 17);
    if (ts_node_is_null(bases)) {
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode child = ts_node_child(node, i);
            if (is_type(child, "base_class_clause")) {
                bases = child;
                break;
            }
        }
    }
    if (!ts_node_is_null(bases)) collect_bases(bases, record);

    collect_members(body, record, record.kind == "class" ? "private" : "public");

    if (!record.name.empty()) unit_.structs.push_back(std::move(record));
}

void UnitBuilder::visit_enum(TSNode node) {
    TSNode body = ts_node_child_by_field_name(node, "body", 4);
    TSNode name = ts_node_child_by_field_name(node, "name", 4);
    if (ts_node_is_null(body)) {
        if (has_annotations(node)) {
            std::vector<AnnotationRef> claimed = take_annotations(node);
            diags_.error("annotated-forward-declaration",
                         "annotation '" + claimed.front().name +
                             "' is on a declaration with no body",
                         "annotate the definition of the enum instead", src_,
                         ts_node_start_byte(node));
        }
        return;
    }

    Enum e;
    e.ns = scope_;
    e.name = ts_node_is_null(name) ? "" : text(name);
    e.annotations = take_annotations(node);
    e.loc = loc(node);

    TSNode base = ts_node_child_by_field_name(node, "base", 4);
    if (!ts_node_is_null(base)) e.underlying = normalize_spelling(text(base));

    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_child(node, i);
        const char* t = ts_node_type(child);
        if (std::strcmp(t, "class") == 0 || std::strcmp(t, "struct") == 0) e.is_scoped = true;
    }

    uint32_t values = ts_node_named_child_count(body);
    for (uint32_t i = 0; i < values; ++i) {
        TSNode child = ts_node_named_child(body, i);
        if (!is_type(child, "enumerator")) continue;
        EnumValue value;
        TSNode vname = ts_node_child_by_field_name(child, "name", 4);
        if (!ts_node_is_null(vname)) value.name = text(vname);
        TSNode vvalue = ts_node_child_by_field_name(child, "value", 5);
        if (!ts_node_is_null(vvalue)) value.value = normalize_spelling(text(vvalue));
        e.values.push_back(std::move(value));
    }

    if (e.name.empty()) {
        if (!e.annotations.empty()) {
            diags_.error("annotated-anonymous-type", "annotation on an enum with no name",
                         "give the enum a name", src_, ts_node_start_byte(node));
        }
        return;
    }
    unit_.enums.push_back(std::move(e));
}

void UnitBuilder::visit_declaration(TSNode node) {
    std::vector<TSNode> declarators = declarators_of(node);
    if (declarators.empty()) return;

    uint32_t prefix_begin = ts_node_start_byte(type_start_node(node));
    uint32_t prefix_end = ts_node_start_byte(declarators.front());
    if (prefix_end < prefix_begin) prefix_end = prefix_begin;

    std::vector<AnnotationRef> annotations = take_annotations(node);

    for (TSNode declarator : declarators) {
        TSNode func = find_function_declarator(declarator);
        if (ts_node_is_null(func)) continue;
        TSNode inner = ts_node_child_by_field_name(func, "declarator", 10);
        if (ts_node_is_null(inner) || !is_declared_name(inner)) continue;

        TSNode id = innermost_declarator_id(declarator);

        Function fn;
        fn.ns = scope_;
        fn.name = text(id);
        fn.annotations = annotations;
        fn.loc = loc(node);
        collect_params(ts_node_child_by_field_name(func, "parameters", 10), fn.params);

        std::string return_type(src_.slice(prefix_begin, prefix_end));
        return_type += std::string(src_.slice(ts_node_start_byte(declarator),
                                              ts_node_start_byte(id)));
        fn.return_type = normalize_spelling(return_type);

        uint32_t func_children = ts_node_child_count(func);
        for (uint32_t i = 0; i < func_children; ++i) {
            TSNode child = ts_node_child(func, i);
            if (is_type(child, "trailing_return_type")) {
                std::string trailing = normalize_spelling(text(child));
                if (trailing.rfind("->", 0) == 0) {
                    trailing = normalize_spelling(trailing.substr(2));
                }
                fn.return_type = trailing;
            }
        }

        unit_.functions.push_back(std::move(fn));
    }
}

void UnitBuilder::visit_template(TSNode node) {
    if (has_annotations(node)) {
        std::vector<AnnotationRef> claimed = take_annotations(node);
        diags_.error("annotated-template",
                     "annotation '" + claimed.front().name + "' is on a template",
                     "templates are not supported; annotate an explicit instantiation, or write "
                     "the reflection data for this type by hand",
                     src_, ts_node_start_byte(node));
    }
    // The body is not walked. Its annotations are marked attached so they do
    // not each report on top of the one error that matters.
    uint32_t a = ts_node_start_byte(node);
    uint32_t b = ts_node_end_byte(node);
    for (size_t i = 0; i < prepass_.sites.size(); ++i) {
        if (prepass_.sites[i].begin >= a && prepass_.sites[i].begin < b) attached_[i] = true;
    }
}

void UnitBuilder::walk_scope(TSNode node) {
    uint32_t count = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < count; ++i) {
        TSNode child = ts_node_named_child(node, i);
        const char* t = ts_node_type(child);

        if (std::strcmp(t, "preproc_include") == 0) {
            collect_include(child);
        } else if (std::strcmp(t, "namespace_definition") == 0) {
            TSNode name = ts_node_child_by_field_name(child, "name", 4);
            std::string saved = scope_;
            if (!ts_node_is_null(name)) scope_ = scope_name(text(name));
            TSNode body = ts_node_child_by_field_name(child, "body", 4);
            if (!ts_node_is_null(body)) walk_scope(body);
            scope_ = saved;
        } else if (std::strcmp(t, "struct_specifier") == 0 ||
                   std::strcmp(t, "class_specifier") == 0 ||
                   std::strcmp(t, "union_specifier") == 0) {
            visit_record(child);
        } else if (std::strcmp(t, "enum_specifier") == 0) {
            visit_enum(child);
        } else if (std::strcmp(t, "template_declaration") == 0) {
            visit_template(child);
        } else if (std::strcmp(t, "declaration") == 0) {
            if (ts_node_is_null(visit_inline_type(child, ""))) visit_declaration(child);
        } else if (std::strcmp(t, "linkage_specification") == 0 ||
                   std::strcmp(t, "declaration_list") == 0 ||
                   std::strcmp(t, "preproc_if") == 0 || std::strcmp(t, "preproc_ifdef") == 0 ||
                   std::strcmp(t, "preproc_else") == 0 || std::strcmp(t, "preproc_elif") == 0) {
            // tree-sitter does not evaluate conditions, so both arms are walked.
            walk_scope(child);
        }
    }
}

// The first token of the declaration containing `at`, if that declaration
// starts before the annotation. Empty otherwise.
std::string UnitBuilder::absorbing_token(TSNode root, uint32_t at,
                                         uint32_t annotation_begin) const {
    TSNode node = ts_node_descendant_for_byte_range(root, at, at);
    while (!ts_node_is_null(node)) {
        uint32_t begin = ts_node_start_byte(node);
        if (begin < annotation_begin) {
            TSNode first = node;
            while (ts_node_child_count(first) > 0) first = ts_node_child(first, 0);
            std::string spelling = text(first);
            // Only an identifier is worth naming.
            if (!spelling.empty() && (std::isalpha(static_cast<unsigned char>(spelling[0])) ||
                                      spelling[0] == '_')) {
                return spelling;
            }
            return {};
        }
        node = ts_node_parent(node);
    }
    return {};
}

// Reports every annotation no declaration claimed, telling the causes apart.
void UnitBuilder::report_unattached(TSNode root) {
    for (size_t i = 0; i < prepass_.sites.size(); ++i) {
        if (attached_[i]) continue;
        const AnnotationSite& site = prepass_.sites[i];
        uint32_t at = site.attaches_to;

        TSNode node = TSNode{};
        if (at < src_.size()) {
            node = ts_node_descendant_for_byte_range(root, at, at);
            // Several nodes can start at the same byte. The outermost is the
            // declaration.
            while (!ts_node_is_null(node)) {
                TSNode parent = ts_node_parent(node);
                if (ts_node_is_null(parent) || ts_node_start_byte(parent) != at) break;
                node = parent;
            }
        }

        bool on_declaration = false;
        if (!ts_node_is_null(node) && ts_node_start_byte(node) == at) {
            const char* t = ts_node_type(node);
            size_t len = std::strlen(t);
            auto ends_with = [&](const char* suffix) {
                size_t n = std::strlen(suffix);
                return len >= n && std::strcmp(t + len - n, suffix) == 0;
            };
            on_declaration = ends_with("declaration") || ends_with("specifier") ||
                             ends_with("declarator") || ends_with("definition");
        }

        if (!on_declaration) {
            // Usually a macro in front of the declaration, which reads as the
            // start of one big declaration that swallows what follows.
            std::string absorber = absorbing_token(root, at, site.begin);
            if (!absorber.empty()) {
                diags_.error("absorbed-declaration",
                             "the declaration annotated with '" + site.name +
                                 "' is swallowed by a declaration that starts at '" + absorber +
                                 "'",
                             "if '" + absorber +
                                 "' is a macro, declare it with reflect_ignore_macro(\"" +
                                 absorber + "\") so the parser can skip over it",
                             src_, site.begin);
            } else {
                diags_.error("unattached-annotation",
                             "annotation '" + site.name + "' is not attached to a declaration",
                             "an annotation must come directly before the declaration it "
                             "describes",
                             src_, site.begin);
            }
        } else if (ts_node_has_error(node)) {
            diags_.error("unparsable-declaration",
                         "the declaration annotated with '" + site.name +
                             "' could not be parsed",
                         "a macro in type position, or a construct the syntactic parser does not "
                         "model; simplify the declaration or write its reflection data by hand",
                         src_, site.begin);
        } else {
            diags_.error("unsupported-declaration",
                         "the declaration annotated with '" + site.name +
                             "' is not a kind xrefl reflects",
                         "annotations apply to structs, classes, unions, enums, fields, methods "
                         "and free functions",
                         src_, site.begin);
        }
    }
}

}  // namespace

bool parse_unit(const Source& source, const std::vector<std::string>& annotation_names,
                const std::vector<std::string>& ignored_macros, Unit& unit, DiagSink& diags) {
    Prepass prepass = run_prepass(source, annotation_names, ignored_macros, diags);

    TSParser* parser = ts_parser_new();
    if (!ts_parser_set_language(parser, tree_sitter_cpp())) {
        ts_parser_delete(parser);
        diags.error("grammar-mismatch", "the vendored C++ grammar is not compatible with the "
                                        "tree-sitter runtime",
                    "rebuild xrefl, or re-run scripts/update-vendor.sh", source, 0);
        return false;
    }

    TSTree* tree = ts_parser_parse_string(parser, nullptr, prepass.blanked.c_str(),
                                          static_cast<uint32_t>(prepass.blanked.size()));
    if (!tree) {
        ts_parser_delete(parser);
        diags.error("parse-failed", "the header could not be parsed", "", source, 0);
        return false;
    }

    unit.path = source.path();

    TSNode root = ts_tree_root_node(tree);
    UnitBuilder builder(source, prepass, unit, diags);
    builder.walk_scope(root);
    builder.report_unattached(root);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return true;
}

}  // namespace xrefl
