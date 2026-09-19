-- A name table per reflected struct. Consumers inherit this emitter.

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

    out.header:write("#pragma once\n#include \"%s\"\n\n", unit.include_path)
    for _, record in ipairs(reflected) do
        out.header:write("const char* const* field_names_%s(int* count);\n", record.symbol)

        out:write("static const char* const %s_names[] = {\n", record.symbol)
        local count = 0
        for _, field in ipairs(record.fields) do
            if field.annotations.PROPERTY then
                out:write("    \"%s\",\n", field.name)
                count = count + 1
            end
        end
        out:write("    nullptr,\n};\n\n")
        out:write("const char* const* field_names_%s(int* count) {\n", record.symbol)
        out:write("    *count = %d;\n    return %s_names;\n}\n\n", count, record.symbol)
    end
end
