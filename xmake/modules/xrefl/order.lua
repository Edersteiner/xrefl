-- Byte order for anything whose order reaches generated output. Lua compares
-- strings with the process locale's collation, which xmake takes from the
-- environment, so two machines would otherwise generate different files from
-- the same headers.

function bytewise(a, b)
    local n = math.min(#a, #b)
    for i = 1, n do
        local x, y = a:byte(i), b:byte(i)
        if x ~= y then
            return x < y
        end
    end
    return #a < #b
end

function sort(list)
    table.sort(list, bytewise)
    return list
end
