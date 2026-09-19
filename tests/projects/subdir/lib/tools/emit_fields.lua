-- One field table per reflected struct, plus a registry in the target phase.

function emit(unit, out)
    local reflected = {}
    for _, record in ipairs(unit.structs) do
        if record.annotations.REFLECT then
            table.insert(reflected, record)
        end
    end
    if #reflected == 0 then
        return
    end

    out.header:write("#pragma once\n#include \"xrefl_runtime.h\"\n")
    out.header:write("#include \"%s\"\n\n", unit.include_path)

    for _, record in ipairs(reflected) do
        do
            local fields = {}
            for _, field in ipairs(record.fields) do
                if field.annotations.PROPERTY then
                    table.insert(fields, field)
                end
            end

            out.header:write("const xrefl::TypeInfo& type_info_%s();\n", record.symbol)

            out:write("static const xrefl::FieldInfo %s_fields[] = {\n", record.symbol)
            for _, field in ipairs(fields) do
                out:write("    { \"%s\", offsetof(%s, %s), sizeof(decltype(%s::%s)), %s },\n",
                          field.name, record.qualified_name, field.name,
                          record.qualified_name, field.name,
                          field.annotations.PROPERTY.transient and "true" or "false")
            end
            out:write("};\n\n")

            -- The range bounds are written as numbers on purpose: they must
            -- come out with a full stop whatever the locale.
            local low, high = 0, 0
            for _, field in ipairs(fields) do
                local range = field.annotations.PROPERTY.range
                if range then
                    low, high = range[1], range[2]
                end
            end
            out:write("const xrefl::TypeInfo& type_info_%s() {\n", record.symbol)
            out:write("    static const xrefl::TypeInfo info{ \"%s\", \"%s\", %s_fields, %d, %s, %s };\n",
                      record.qualified_name, record.annotations.REFLECT.category or "",
                      record.symbol, #fields, low, high)
            out:write("    return info;\n}\n\n")
        end
    end
end

function emit_target(units, out)
    local symbols = {}
    for _, unit in ipairs(units) do
        for _, record in ipairs(unit.structs) do
            if record.annotations.REFLECT then
                table.insert(symbols, record.symbol)
            end
        end
    end

    out.header:write("\nconst xrefl::TypeInfo* const* xrefl_all_types(int* count);\n")
    -- A definition, so including the aggregate twice would be a compile error
    -- if it did not guard itself.
    out.header:write("inline constexpr int xrefl_type_count = %d;\n", #symbols)
    out:write("static const xrefl::TypeInfo* const all_types[] = {\n")
    for _, symbol in ipairs(symbols) do
        out:write("    &type_info_%s(),\n", symbol)
    end
    out:write("};\n\n")
    out:write("const xrefl::TypeInfo* const* xrefl_all_types(int* count) {\n")
    out:write("    *count = %d;\n    return all_types;\n}\n", #symbols)
end
