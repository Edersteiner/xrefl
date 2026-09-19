-- Matches base classes against the target's reflected types. Runs once all
-- units are loaded, since `Entity` inside `namespace game` can only be matched
-- knowing every type in the target.

-- Candidate qualified names, innermost scope outwards.
function _candidates(spelled, scope)
    local name = spelled:gsub("^%s*::%s*", "")
    -- A template base matches by its template name.
    name = name:gsub("%s*<.*$", "")

    local out = {}
    if scope and scope ~= "" then
        local prefix = scope
        while prefix and prefix ~= "" do
            table.insert(out, prefix .. "::" .. name)
            local cut = prefix:match("^(.*)::[^:]+$")
            prefix = cut
        end
    end
    table.insert(out, name)
    return out
end

-- Sets `resolved` on every base that names a reflected type. An unmatched
-- base is normal, it may come from a library.
function resolve(units)
    local known = {}
    local owner = {}
    for _, unit in ipairs(units) do
        for _, record in ipairs(unit.structs) do
            known[record.qualified_name] = record
            owner[record.qualified_name] = unit
        end
    end

    for _, unit in ipairs(units) do
        for _, record in ipairs(unit.structs) do
            for _, base in ipairs(record.bases) do
                for _, candidate in ipairs(_candidates(base.name, record.namespace)) do
                    if known[candidate] then
                        base.resolved = candidate
                        base.record = known[candidate]
                        -- Generated code referring to the base has to include
                        -- the header that declares it.
                        local from = owner[candidate]
                        if from ~= unit then
                            base.generated_header = from.generated_header
                        end
                        break
                    end
                end
            end
        end
    end
    return known
end
