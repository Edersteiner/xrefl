-- Round-trips real headers. Parses, inserts an annotation at every reported
-- declaration, parses again, and requires each one to attach to the same
-- declaration.

import("core.base.json")
import("report")

local ANNOTATIONS = "REFLECT,PROPERTY,METHOD"

function _parse(xrefl, file)
    local output, code = report.capture(xrefl, {"parse", "--annotations", ANNOTATIONS,
                                                "--stdout", file})
    if code ~= 0 then
        return nil, output:match("^[^\n]*") or ""
    end
    return json.decode(output)
end

-- Last position first, so earlier offsets stay valid.
function _inject(text, insertions)
    local lines = {}
    for line in (text .. "\n"):gmatch("(.-)\n") do
        table.insert(lines, line)
    end
    table.sort(insertions, function (x, y)
        if x.line ~= y.line then
            return x.line > y.line
        end
        return x.column > y.column
    end)
    for _, at in ipairs(insertions) do
        local row = lines[at.line]
        if row then
            local cut = math.min(at.column - 1, #row)
            lines[at.line] = row:sub(1, cut) .. at.marker .. row:sub(cut + 1)
        end
    end
    return table.concat(lines, "\n")
end

-- Names as well as counts, to catch an annotation landing on the wrong
-- declaration.
function _signature(unit)
    local out = {}
    for _, record in ipairs(unit.structs) do
        local fields, methods = {}, {}
        for _, f in ipairs(record.fields) do table.insert(fields, f.name) end
        for _, m in ipairs(record.methods) do table.insert(methods, m.name) end
        table.sort(fields)
        table.sort(methods)
        table.insert(out, record.namespace .. "|" .. record.name .. "|" ..
                     table.concat(fields, ",") .. "|" .. table.concat(methods, ","))
    end
    table.sort(out)
    return table.concat(out, "\n")
end

function _annotated_count(unit)
    local structs, members = 0, 0
    for _, record in ipairs(unit.structs) do
        if #record.annotations > 0 then structs = structs + 1 end
        for _, f in ipairs(record.fields) do
            if #f.annotations > 0 then members = members + 1 end
        end
        for _, m in ipairs(record.methods) do
            if #m.annotations > 0 then members = members + 1 end
        end
    end
    return structs, members
end

function main(xrefl, opt)
    if #opt.files == 0 then
        raise("xmake check corpus needs header files to run against")
    end

    local checked, skipped, mismatched = 0, 0, 0
    local failures = {}
    local scratch = path.join(os.tmpdir(), "xrefl-roundtrip.h")

    for _, header in ipairs(opt.files) do
        local original = _parse(xrefl, header)
        local insertions = {}
        local expected_structs, expected_members = 0, 0
        if original then
            for _, record in ipairs(original.structs) do
                table.insert(insertions, {line = record.line, column = record.column,
                                          marker = "REFLECT() "})
                expected_structs = expected_structs + 1
                for _, f in ipairs(record.fields) do
                    table.insert(insertions, {line = f.line, column = f.column,
                                              marker = "PROPERTY() "})
                end
                for _, m in ipairs(record.methods) do
                    table.insert(insertions, {line = m.line, column = m.column,
                                              marker = "METHOD() "})
                end
                expected_members = expected_members + #record.fields + #record.methods
            end
        end

        local text = original and io.readfile(header)
        if not text or #insertions == 0 then
            skipped = skipped + 1
        else
            io.writefile(scratch, _inject(text, insertions))
            local result, err = _parse(xrefl, scratch)
            checked = checked + 1

            local reason
            if not result then
                reason = err
            else
                local structs, members = _annotated_count(result)
                if #result.structs ~= expected_structs or structs ~= expected_structs then
                    reason = ("structs %d/%d"):format(structs, expected_structs)
                elseif members ~= expected_members then
                    reason = ("members %d/%d"):format(members, expected_members)
                elseif _signature(result) ~= _signature(original) then
                    reason = "declaration names differ after annotation"
                end
            end
            if reason then
                mismatched = mismatched + 1
                table.insert(failures, header .. ": " .. reason)
            end
        end
    end
    os.tryrm(scratch)

    print("checked %d headers, skipped %d with no declarations", checked, skipped)
    print("mismatched: %d", mismatched)
    for i = 1, math.min(#failures, 25) do
        print("  %s", failures[i])
    end
    if #failures > 25 then
        print("  ... and %d more", #failures - 25)
    end
    return mismatched == 0
end
