-- Reference emitter: JSON serialization through yyjson. Copy it and change
-- it. Generates against runtime/xrefl/json_yyjson.h.

-- Byte order, so the output is the same on every machine.
import("xrefl.order")

local TYPE_ANNOTATION = "REFLECT"
local FIELD_ANNOTATION = "PROPERTY"

-- REFLECT(polymorphic) opts a type in. Reflected types deriving from
-- it are registered too.
local POLYMORPHIC_ARG = "polymorphic"

-- `PROPERTY(transient)` keeps a field out of the serialized form.
function _serialized_fields(record)
    local fields = {}
    for _, field in ipairs(record.fields) do
        local annotation = field.annotations[FIELD_ANNOTATION]
        if annotation and not annotation.transient and not field.is_static
                and field.bitfield == "" then
            table.insert(fields, field)
        end
    end
    return fields
end

function _is_polymorphic(record)
    local seen = {}
    local function walk(current)
        if not current or seen[current.qualified_name] then
            return false
        end
        seen[current.qualified_name] = true
        local annotation = current.annotations[TYPE_ANNOTATION]
        if annotation and annotation[POLYMORPHIC_ARG] then
            return true
        end
        for _, base in ipairs(current.bases) do
            if base.record and walk(base.record) then
                return true
            end
        end
        return false
    end
    return walk(record)
end

function _reflected_records(unit)
    local records = {}
    for _, record in ipairs(unit.structs) do
        if record.annotations[TYPE_ANNOTATION] then
            table.insert(records, record)
        end
    end
    return records
end

function _reflected_enums(unit)
    local enums = {}
    for _, item in ipairs(unit.enums) do
        if item.annotations[TYPE_ANNOTATION] then
            table.insert(enums, item)
        end
    end
    return enums
end

-- A scoped enum qualifies its enumerators by the enum name, an unscoped one
-- does not.
function _enumerator(item, value)
    if item.is_scoped or item.scoped then
        return item.qualified_name .. "::" .. value.name
    end
    if item.namespace ~= "" then
        return item.namespace .. "::" .. value.name
    end
    return value.name
end

-- Generated functions live in the type's namespace so ADL finds them.
function _open_namespace(out, namespace)
    if namespace == "" then
        return
    end
    for part in namespace:gmatch("[^:]+") do
        out:write("namespace %s { ", part)
    end
    out:write("\n")
end

function _close_namespace(out, namespace)
    if namespace == "" then
        return
    end
    local depth = 0
    for _ in namespace:gmatch("[^:]+") do
        depth = depth + 1
    end
    out:write("%s  // namespace %s\n", string.rep("}", depth), namespace)
end

function _emit_enum(item, out)
    _open_namespace(out.header, item.namespace)
    out.header:write("yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, %s value);\n",
                     item.qualified_name)
    out.header:write("bool xrefl_from_json(yyjson_val* value, %s& out);\n", item.qualified_name)
    _close_namespace(out.header, item.namespace)

    _open_namespace(out, item.namespace)
    out:write("yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, %s value) {\n",
              item.qualified_name)
    out:write("    switch (value) {\n")
    for _, entry in ipairs(item.values) do
        out:write("        case %s: return yyjson_mut_str(doc, \"%s\");\n",
                  _enumerator(item, entry), entry.name)
    end
    out:write("    }\n    return yyjson_mut_null(doc);\n}\n\n")

    out:write("bool xrefl_from_json(yyjson_val* value, %s& out) {\n", item.qualified_name)
    out:write("    const char* text = yyjson_get_str(value);\n")
    out:write("    if (!text) return false;\n")
    for _, entry in ipairs(item.values) do
        out:write("    if (std::strcmp(text, \"%s\") == 0) { out = %s; return true; }\n",
                  entry.name, _enumerator(item, entry))
    end
    out:write("    return false;\n}\n")
    _close_namespace(out, item.namespace)
    out:write("\n")
end

function _emit_record(record, out)
    local name = record.qualified_name
    local fields = _serialized_fields(record)

    -- Only a reflected base has a generated pair to call.
    local base = nil
    for _, candidate in ipairs(record.bases) do
        if candidate.resolved and candidate.access == "public" then
            base = candidate
            break
        end
    end

    _open_namespace(out.header, record.namespace)
    -- The `_fields` form writes into an existing object, so a derived type can
    -- call its base's first and share one object.
    out.header:write("void xrefl_to_json_fields(yyjson_mut_doc* doc, yyjson_mut_val* obj, " ..
                     "const %s& value);\n", name)
    out.header:write("yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, const %s& value);\n",
                     name)
    out.header:write("bool xrefl_from_json(yyjson_val* obj, %s& out);\n", name)
    _close_namespace(out.header, record.namespace)

    _open_namespace(out, record.namespace)
    out:write("void xrefl_to_json_fields(yyjson_mut_doc* doc, yyjson_mut_val* obj, " ..
              "const %s& value) {\n", name)
    if base then
        out:write("    xrefl_to_json_fields(doc, obj, static_cast<const %s&>(value));\n",
                  base.resolved)
    end
    for _, field in ipairs(fields) do
        out:write("    yyjson_mut_obj_add_val(doc, obj, \"%s\", " ..
                  "xrefl::json::write(doc, value.%s));\n", field.name, field.name)
    end
    if #fields == 0 and not base then
        out:write("    (void)doc; (void)obj; (void)value;\n")
    end
    out:write("}\n\n")

    out:write("yyjson_mut_val* xrefl_to_json(yyjson_mut_doc* doc, const %s& value) {\n", name)
    out:write("    yyjson_mut_val* obj = yyjson_mut_obj(doc);\n")
    out:write("    xrefl_to_json_fields(doc, obj, value);\n")
    out:write("    return obj;\n}\n\n")

    out:write("bool xrefl_from_json(yyjson_val* obj, %s& out) {\n", name)
    out:write("    if (!yyjson_is_obj(obj)) return false;\n")
    if base then
        out:write("    if (!xrefl_from_json(obj, static_cast<%s&>(out))) return false;\n",
                  base.resolved)
    end
    for _, field in ipairs(fields) do
        -- A missing key is an older file and keeps the default. A wrong type
        -- is a real mismatch.
        out:write("    if (yyjson_val* f = yyjson_obj_get(obj, \"%s\")) {\n", field.name)
        out:write("        if (!xrefl::json::read(f, out.%s)) return false;\n", field.name)
        out:write("    }\n")
    end
    out:write("    return true;\n}\n")
    _close_namespace(out, record.namespace)
    out:write("\n")

    if _is_polymorphic(record) then
        _emit_polymorphic(record, base, out)
    end
end

-- Plain functions, so the registry can be a static table of pointers.
function _emit_polymorphic(record, base, out)
    local name = record.qualified_name
    local symbol = record.symbol

    out.header:write("bool xrefl_json_derives_%s(xrefl::TypeId target);\n", symbol)
    out.header:write("void* xrefl_json_upcast_%s(void* self, xrefl::TypeId target);\n", symbol)
    out.header:write("void* xrefl_json_create_%s();\n", symbol)
    out.header:write("void xrefl_json_destroy_%s(void* self);\n", symbol)
    out.header:write("bool xrefl_json_read_%s(yyjson_val* obj, void* self);\n", symbol)
    out.header:write("yyjson_mut_val* xrefl_json_write_%s(yyjson_mut_doc* doc, " ..
                     "const void* self);\n", symbol)

    out:write("// %s, registered for loading through a base pointer\n", name)
    out:write("static_assert(std::is_default_constructible<%s>::value,\n", name)
    out:write("              \"%s is polymorphic, so it must be default-constructible " ..
              "to be created from a document\");\n", name)

    -- Answers without an object, so a mismatch is rejected before anything is
    -- constructed.
    out:write("bool xrefl_json_derives_%s(xrefl::TypeId target) {\n", symbol)
    out:write("    if (target == xrefl::type_id<%s>()) return true;\n", name)
    if base then
        out:write("    return xrefl_json_derives_%s(target);\n",
                  (base.resolved:gsub("::", "_")))
    else
        out:write("    return false;\n")
    end
    out:write("}\n\n")

    out:write("void* xrefl_json_upcast_%s(void* self, xrefl::TypeId target) {\n", symbol)
    out:write("    if (target == xrefl::type_id<%s>()) return self;\n", name)
    if base then
        -- Cast where both types are complete, so a base at a non-zero offset
        -- works.
        out:write("    %s* derived = static_cast<%s*>(self);\n", name, name)
        out:write("    return xrefl_json_upcast_%s(static_cast<%s*>(derived), target);\n",
                  (base.resolved:gsub("::", "_")), base.resolved)
    else
        out:write("    return nullptr;\n")
    end
    out:write("}\n\n")

    out:write("void* xrefl_json_create_%s() { return new %s(); }\n", symbol, name)
    out:write("void xrefl_json_destroy_%s(void* self) { delete static_cast<%s*>(self); }\n",
              symbol, name)
    out:write("bool xrefl_json_read_%s(yyjson_val* obj, void* self) {\n", symbol)
    out:write("    return xrefl_from_json(obj, *static_cast<%s*>(self));\n}\n", name)
    out:write("yyjson_mut_val* xrefl_json_write_%s(yyjson_mut_doc* doc, const void* self) {\n",
              symbol)
    out:write("    yyjson_mut_val* obj = yyjson_mut_obj(doc);\n")
    out:write("    yyjson_mut_obj_add_str(doc, obj, xrefl::json::discriminator(), \"%s\");\n",
              name)
    out:write("    xrefl_to_json_fields(doc, obj, *static_cast<const %s*>(self));\n", name)
    out:write("    return obj;\n}\n\n")
end

function emit(unit, out)
    local records = _reflected_records(unit)
    local enums = _reflected_enums(unit)
    if #records == 0 and #enums == 0 then
        return
    end

    out.header:write("#pragma once\n")
    out.header:write("#include <cstring>\n")
    out.header:write("#include <type_traits>\n")
    out.header:write("#include <xrefl/json_yyjson.h>\n")
    out.header:write("#include \"%s\"\n", unit.include_path)

    local needed = {}
    for _, record in ipairs(records) do
        for _, base in ipairs(record.bases) do
            if base.generated_header then
                needed[base.generated_header] = true
            end
        end
    end
    local sorted = {}
    for header in pairs(needed) do
        table.insert(sorted, header)
    end
    order.sort(sorted)
    for _, header in ipairs(sorted) do
        out.header:write("#include \"%s\"\n", header)
    end
    out.header:write("\n")

    for _, item in ipairs(enums) do
        _emit_enum(item, out)
    end
    for _, record in ipairs(records) do
        _emit_record(record, out)
    end
end

function emit_target(units, out)
    local records = {}
    for _, unit in ipairs(units) do
        for _, record in ipairs(_reflected_records(unit)) do
            if _is_polymorphic(record) then
                table.insert(records, record)
            end
        end
    end
    if #records == 0 then
        return
    end
    table.sort(records, function (a, b) return order.bytewise(a.qualified_name, b.qualified_name) end)

    out.header:write("\n#include <xrefl/json_yyjson.h>\n")
    out.header:write("\n// Every polymorphic type reflected in this target.\n")
    out.header:write("const xrefl::json::PolymorphicRegistry& xrefl_json_registry();\n")

    out:write("static const xrefl::json::PolymorphicEntry xrefl_json_entries[] = {\n")
    for _, record in ipairs(records) do
        local name = record.qualified_name
        local symbol = record.symbol
        out:write("    { \"%s\", xrefl::type_id<%s>(),\n", name, name)
        out:write("      &xrefl_json_create_%s, &xrefl_json_destroy_%s,\n", symbol, symbol)
        out:write("      &xrefl_json_read_%s, &xrefl_json_write_%s,\n", symbol, symbol)
        out:write("      &xrefl_json_derives_%s, &xrefl_json_upcast_%s,\n", symbol, symbol)
        out:write("#ifndef XREFL_JSON_NO_RTTI\n")
        out:write("      &typeid(%s),\n", name)
        out:write("#endif\n")
        out:write("    },\n")
    end
    out:write("};\n\n")
    out:write("const xrefl::json::PolymorphicRegistry& xrefl_json_registry() {\n")
    out:write("    static const xrefl::json::PolymorphicRegistry registry{" ..
              "xrefl_json_entries, %d};\n", #records)
    out:write("    return registry;\n}\n")
end
