-- Reference emitter: field tables and type registration. Copy it and change
-- it. Generates against runtime/xrefl/registry.h, which is an example too.

-- Byte order, so the output is the same on every machine.
import("xrefl.order")

-- Change these to match your scheme.
local TYPE_ANNOTATION = "REFLECT"
local FIELD_ANNOTATION = "PROPERTY"

function _reflected_fields(record)
    local fields = {}
    for _, field in ipairs(record.fields) do
        -- Statics have no offset and bitfields have no address.
        if field.annotations[FIELD_ANNOTATION] and not field.is_static
                and field.bitfield == "" then
            table.insert(fields, field)
        end
    end
    return fields
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

function emit(unit, out)
    local records = _reflected_records(unit)
    if #records == 0 then
        return
    end

    out.header:write("#pragma once\n")
    out.header:write("#include <xrefl/registry.h>\n")
    out.header:write("#include \"%s\"\n", unit.include_path)

    -- A base from another header needs that header's generated declarations.
    local needed = {}
    for _, record in ipairs(records) do
        for _, base in ipairs(record.bases) do
            if base.generated_header then
                needed[base.generated_header] = true
            end
        end
    end
    local sorted = {}
    for name in pairs(needed) do
        table.insert(sorted, name)
    end
    order.sort(sorted)
    for _, name in ipairs(sorted) do
        out.header:write("#include \"%s\"\n", name)
    end
    out.header:write("\n")

    for _, record in ipairs(records) do
        out.header:write("const xrefl::TypeInfo& xrefl_type_%s();\n", record.symbol)
    end

    -- offsetof on a type with a base class is conditionally supported and
    -- gcc and clang warn about it. Every mainstream compiler handles it.
    out:write("#if defined(__GNUC__) || defined(__clang__)\n")
    out:write("#pragma GCC diagnostic push\n")
    out:write("#pragma GCC diagnostic ignored \"-Winvalid-offsetof\"\n")
    out:write("#endif\n")

    for _, record in ipairs(records) do
        local name = record.qualified_name
        local fields = _reflected_fields(record)

        -- Only reflected bases can be linked to.
        local linked_bases = {}
        for _, base in ipairs(record.bases) do
            if base.resolved then
                table.insert(linked_bases, base)
            end
        end

        out:write("\n// %s\n", name)
        if #linked_bases > 0 then
            out:write("static const xrefl::TypeInfo* const %s_bases[] = {\n", record.symbol)
            for _, base in ipairs(linked_bases) do
                out:write("    &xrefl_type_%s(),\n", (base.resolved:gsub("::", "_")))
            end
            out:write("};\n")
        end

        out:write("static const xrefl::FieldInfo %s_fields[] = {\n", record.symbol)
        for _, field in ipairs(fields) do
            -- The compiler answers offsetof and sizeof. This only spells the
            -- types the way the header did.
            out:write("    { \"%s\", \"%s\", xrefl::type_id<decltype(%s::%s)>(),\n",
                      field.name, field.type, name, field.name)
            out:write("      offsetof(%s, %s), sizeof(decltype(%s::%s)) },\n",
                      name, field.name, name, field.name)
        end
        out:write("};\n")

        out:write("const xrefl::TypeInfo& xrefl_type_%s() {\n", record.symbol)
        out:write("    static const xrefl::TypeInfo info{\n")
        out:write("        \"%s\", xrefl::type_id<%s>(), sizeof(%s),\n", name, name, name)
        if #linked_bases > 0 then
            out:write("        %s_bases, %d,\n", record.symbol, #linked_bases)
        else
            out:write("        nullptr, 0,\n")
        end
        out:write("        %s_fields, %d};\n", record.symbol, #fields)
        out:write("    return info;\n}\n")
    end

    out:write("\n#if defined(__GNUC__) || defined(__clang__)\n")
    out:write("#pragma GCC diagnostic pop\n")
    out:write("#endif\n")
end

function emit_target(units, out)
    local records = {}
    for _, unit in ipairs(units) do
        for _, record in ipairs(_reflected_records(unit)) do
            table.insert(records, record)
        end
    end
    table.sort(records, function (a, b) return order.bytewise(a.qualified_name, b.qualified_name) end)

    out.header:write("\n#include <xrefl/registry.h>\n")
    out.header:write("\n// Every type reflected in this target.\n")
    out.header:write("const xrefl::Registry& xrefl_registry();\n")

    out:write("static const xrefl::TypeInfo* const xrefl_all_types[] = {\n")
    for _, record in ipairs(records) do
        out:write("    &xrefl_type_%s(),\n", record.symbol)
    end
    out:write("    nullptr,\n};\n\n")
    out:write("const xrefl::Registry& xrefl_registry() {\n")
    out:write("    static const xrefl::Registry registry{xrefl_all_types, %d};\n", #records)
    out:write("    return registry;\n}\n")
end
