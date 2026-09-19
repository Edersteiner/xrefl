#include "json.h"

#include <cstdio>
#include <cstring>

namespace xrefl {
namespace {

class Writer {
public:
    explicit Writer(bool pretty) : pretty_(pretty) {}

    std::string take() { return std::move(out_); }

    void begin_object() {
        raw('{');
        ++depth_;
        first_ = true;
    }
    void end_object() {
        --depth_;
        if (!first_) newline();
        first_ = false;
        raw('}');
    }
    void begin_array() {
        raw('[');
        ++depth_;
        first_ = true;
    }
    void end_array() {
        --depth_;
        if (!first_) newline();
        first_ = false;
        raw(']');
    }

    void key(const char* name) {
        separator();
        string(name);
        raw(':');
        if (pretty_) raw(' ');
    }

    void element() { separator(); }

    void string(const std::string& s) { string(s.c_str(), s.size()); }
    void string(const char* s) { string(s, std::strlen(s)); }

    void number(uint32_t v) { out_ += std::to_string(v); }
    void boolean(bool v) { out_ += v ? "true" : "false"; }

private:
    void raw(char c) { out_ += c; }

    void separator() {
        if (!first_) raw(',');
        first_ = false;
        newline();
    }

    void newline() {
        if (!pretty_) return;
        out_ += '\n';
        out_.append(static_cast<size_t>(depth_) * 2, ' ');
    }

    void string(const char* s, size_t n) {
        out_ += '"';
        for (size_t i = 0; i < n; ++i) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            switch (c) {
                case '"': out_ += "\\\""; break;
                case '\\': out_ += "\\\\"; break;
                case '\b': out_ += "\\b"; break;
                case '\f': out_ += "\\f"; break;
                case '\n': out_ += "\\n"; break;
                case '\r': out_ += "\\r"; break;
                case '\t': out_ += "\\t"; break;
                default:
                    if (c < 0x20) {
                        char buf[7];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out_ += buf;
                    } else {
                        out_ += static_cast<char>(c);
                    }
            }
        }
        out_ += '"';
    }

    std::string out_;
    bool pretty_;
    int depth_ = 0;
    bool first_ = true;
};

void write_location(Writer& w, const Location& loc) {
    w.key("line");
    w.number(loc.line);
    w.key("column");
    w.number(loc.column);
}

void write_annotations(Writer& w, const std::vector<AnnotationRef>& annotations) {
    w.key("annotations");
    w.begin_array();
    for (const AnnotationRef& a : annotations) {
        w.element();
        w.begin_object();
        w.key("name");
        w.string(a.name);
        w.key("args");
        w.string(a.args);
        write_location(w, a.loc);
        w.end_object();
    }
    w.end_array();
}

void write_params(Writer& w, const std::vector<Param>& params) {
    w.key("params");
    w.begin_array();
    for (const Param& p : params) {
        w.element();
        w.begin_object();
        w.key("name");
        w.string(p.name);
        w.key("type");
        w.string(p.type);
        w.end_object();
    }
    w.end_array();
}

}  // namespace

std::string to_json(const Unit& unit, bool pretty) {
    Writer w(pretty);
    w.begin_object();

    w.key("path");
    w.string(unit.path);

    w.key("includes");
    w.begin_array();
    for (const std::string& inc : unit.includes) {
        w.element();
        w.string(inc);
    }
    w.end_array();

    w.key("structs");
    w.begin_array();
    for (const Struct& s : unit.structs) {
        w.element();
        w.begin_object();
        w.key("name");
        w.string(s.name);
        w.key("namespace");
        w.string(s.ns);
        w.key("kind");
        w.string(s.kind);
        write_annotations(w, s.annotations);

        w.key("bases");
        w.begin_array();
        for (const Base& b : s.bases) {
            w.element();
            w.begin_object();
            w.key("name");
            w.string(b.name);
            w.key("access");
            w.string(b.access);
            w.key("virtual");
            w.boolean(b.is_virtual);
            w.end_object();
        }
        w.end_array();

        w.key("fields");
        w.begin_array();
        for (const Field& f : s.fields) {
            w.element();
            w.begin_object();
            w.key("name");
            w.string(f.name);
            w.key("type");
            w.string(f.type);
            w.key("access");
            w.string(f.access);
            w.key("static");
            w.boolean(f.is_static);
            w.key("mutable");
            w.boolean(f.is_mutable);
            w.key("bitfield");
            w.string(f.bitfield);
            write_annotations(w, f.annotations);
            write_location(w, f.loc);
            w.end_object();
        }
        w.end_array();

        w.key("methods");
        w.begin_array();
        for (const Method& m : s.methods) {
            w.element();
            w.begin_object();
            w.key("name");
            w.string(m.name);
            w.key("return_type");
            w.string(m.return_type);
            w.key("access");
            w.string(m.access);
            write_params(w, m.params);
            w.key("static");
            w.boolean(m.is_static);
            w.key("virtual");
            w.boolean(m.is_virtual);
            w.key("const");
            w.boolean(m.is_const);
            w.key("pure");
            w.boolean(m.is_pure);
            write_annotations(w, m.annotations);
            write_location(w, m.loc);
            w.end_object();
        }
        w.end_array();

        write_location(w, s.loc);
        w.end_object();
    }
    w.end_array();

    w.key("enums");
    w.begin_array();
    for (const Enum& e : unit.enums) {
        w.element();
        w.begin_object();
        w.key("name");
        w.string(e.name);
        w.key("namespace");
        w.string(e.ns);
        w.key("underlying");
        w.string(e.underlying);
        w.key("scoped");
        w.boolean(e.is_scoped);
        w.key("values");
        w.begin_array();
        for (const EnumValue& v : e.values) {
            w.element();
            w.begin_object();
            w.key("name");
            w.string(v.name);
            w.key("value");
            w.string(v.value);
            w.end_object();
        }
        w.end_array();
        write_annotations(w, e.annotations);
        write_location(w, e.loc);
        w.end_object();
    }
    w.end_array();

    w.key("functions");
    w.begin_array();
    for (const Function& f : unit.functions) {
        w.element();
        w.begin_object();
        w.key("name");
        w.string(f.name);
        w.key("namespace");
        w.string(f.ns);
        w.key("return_type");
        w.string(f.return_type);
        write_params(w, f.params);
        write_annotations(w, f.annotations);
        write_location(w, f.loc);
        w.end_object();
    }
    w.end_array();

    w.end_object();
    std::string out = w.take();
    out += '\n';
    return out;
}

}  // namespace xrefl
