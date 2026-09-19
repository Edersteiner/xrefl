-- Loads emitter scripts and runs them. An emitter has `emit(unit, out)` and
-- optionally `emit_target(units, out)`, which runs once per target.

import("xrefl.writer")

function load_all(scripts)
    local emitters = {}
    for _, script in ipairs(scripts) do
        local module = import(path.basename(script),
                              {rootdir = path.directory(script), anonymous = true})
        if type(module.emit) ~= "function" and type(module.emit_target) ~= "function" then
            raise("xrefl: emitter '%s' defines neither emit() nor emit_target()", script)
        end
        table.insert(emitters, {name = path.basename(script), script = script, module = module})
    end
    return emitters
end

-- All emitters write into the same pair of files for a unit.
function run_unit(emitters, unit)
    local out = writer.new()
    for _, emitter in ipairs(emitters) do
        if emitter.module.emit then
            try
            {
                function ()
                    emitter.module.emit(unit, out)
                end,
                catch
                {
                    function (errors)
                        raise("xrefl: emitter '%s' failed on %s:\n  %s", emitter.name, unit.path,
                              tostring(errors))
                    end
                }
            }
        end
    end
    return out
end

function run_target(emitters, units)
    local out = writer.new()
    for _, emitter in ipairs(emitters) do
        if emitter.module.emit_target then
            try
            {
                function ()
                    emitter.module.emit_target(units, out)
                end,
                catch
                {
                    function (errors)
                        raise("xrefl: emitter '%s' failed in its target phase:\n  %s",
                              emitter.name, tostring(errors))
                    end
                }
            }
        end
    end
    return out
end
