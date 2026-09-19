-- Applies a configuration table to a target. Both ways in share it: the
-- packaged rule takes the table from add_rules("@xrefl/reflect", {...}), the
-- checkout rule from add_rules("xrefl", {...}). Everything is written into
-- the same target values the reflect_* functions use.
--
--   {
--       annotations = { REFLECT = { applies_to = "struct", args = {...} } },
--       emitters = { "@xrefl/emit_registry.lua", "tools/emit_custom.lua" },
--       headers = { "src/**.h" },
--       ignore_macros = { "IMGUI_API" },
--       inherit = { "engine" },
--       publish = true,
--   }
--
-- `emitterdir` is where `@xrefl/<name>.lua` resolves: the reference emitters
-- shipped with the package or in the checkout.
function apply(target, conf, emitterdir)
    conf = conf or {}

    for name, options in pairs(conf.annotations or {}) do
        target:add("values", "xrefl.annotations",
                   string.serialize({name = name, options = options},
                                    {strip = true, indent = false, orderkeys = true}))
    end
    for _, pattern in ipairs(table.wrap(conf.headers)) do
        target:add("values", "xrefl.headers", pattern)
    end
    for _, macro in ipairs(table.wrap(conf.ignore_macros)) do
        target:add("values", "xrefl.ignore_macros", macro)
    end
    for _, name in ipairs(table.wrap(conf.inherit)) do
        target:add("values", "xrefl.inherit", name)
    end
    if conf.publish then
        target:add("values", "xrefl.publish", "true")
    end
    for _, script in ipairs(table.wrap(conf.emitters)) do
        local shipped = script:match("^@xrefl/(.+)$")
        target:add("values", "xrefl.emitters",
                   shipped and path.join(emitterdir, shipped)
                           or path.absolute(script, target:scriptdir()))
    end
end
