-- Works out what will be generated and wires it into the target. Runs at
-- load time, before parsing, which works because there is one generated file
-- per header no matter how many emitters run.

import("xrefl.order")
import("xrefl.paths")

function headers(target)
    local patterns = target:values("xrefl.headers") or {}
    local found = {}
    local seen = {}
    for _, pattern in ipairs(patterns) do
        local absolute = path.absolute(pattern, target:scriptdir())
        for _, file in ipairs(os.files(absolute)) do
            local normalized = path.normalize(file)
            if not seen[normalized] then
                seen[normalized] = true
                table.insert(found, normalized)
            end
        end
    end
    order.sort(found)
    return found
end

-- `relative` is project-relative rather than target-relative because the
-- parser records it in the unit.
function unit_paths(target, header)
    local gendir = paths.gendir(target)
    local relative = path.relative(header, os.projectdir())
    local stem = paths.flatten(relative)
    return {
        header = header,
        relative = relative,
        stem = stem,
        json = path.join(gendir, "units", stem .. ".json"),
        source = path.join(gendir, stem .. ".xrefl.cpp"),
        include = path.join(gendir, stem .. ".xrefl.h"),
    }
end

-- The per-target file for anything that needs every header at once.
function aggregate_paths(target)
    local gendir = paths.gendir(target)
    return {
        source = path.join(gendir, target:name() .. ".xrefl.all.cpp"),
        include = path.join(gendir, target:name() .. ".xrefl.all.h"),
    }
end

function annotations_header(target)
    local relative = path.join(target:name(), "reflect_annotations.h")
    return {
        relative = relative,
        absolute = path.join(paths.includedir(target), relative),
    }
end

-- Staging for what consumers inherit. The `xrefl` level is the install root
-- marker, so it lands at `share/xrefl`.
function payload_dir(target)
    return path.join(paths.gendir(target), "publish", "xrefl")
end

function attach(target)
    local units = {}
    for _, header in ipairs(headers(target)) do
        table.insert(units, unit_paths(target, header))
    end
    local aggregate = aggregate_paths(target)

    -- add_files drops files that do not exist yet unless told otherwise.
    for _, unit in ipairs(units) do
        target:add("files", unit.source, {always_added = true})
    end
    target:add("files", aggregate.source, {always_added = true})

    target:add("includedirs", paths.gendir(target), {public = true})
    target:add("includedirs", paths.includedir(target), {public = true})

    target:data_set("xrefl.units", units)
    target:data_set("xrefl.aggregate", aggregate)
    target:data_set("xrefl.annotations_header", annotations_header(target))

    if target:values("xrefl.publish") then
        local staging = path.join(paths.gendir(target), "publish")
        target:add("installfiles", path.join(staging, "(xrefl/**)"), {prefixdir = "share"})
        target:data_set("xrefl.payload", payload_dir(target))
    end
end
