-- Path conventions shared by the rule and the parser.

-- Joins the segments with dots, dropping leading separators, drive letters
-- and `.`/`..`. Must match output_path() in src/main.cpp, which names the
-- files this predicts.
function flatten(relative)
    local segments = {}
    for segment in relative:gsub("\\", "/"):gmatch("[^/]+") do
        local skip = segment == "." or segment == ".."
        if not skip and #segment == 2 and segment:sub(2, 2) == ":" then
            skip = true
        end
        if not skip then
            table.insert(segments, segment)
        end
    end
    if #segments == 0 then
        return "unit"
    end
    return table.concat(segments, ".")
end

function gendir(target)
    return path.join(target:autogendir(), "rules", "xrefl")
end

-- On the include path, so `<target>/reflect_annotations.h` resolves.
function includedir(target)
    return path.join(gendir(target), "include")
end
