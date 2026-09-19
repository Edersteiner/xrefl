-- Entry point for the checkout form of the plugin.
--
--   includes("path/to/xrefl/xmake/xrefl.lua")
--
--   target("mygame")
--       add_rules("xrefl", {
--           annotations = { REFLECT = { applies_to = "struct" } },
--           emitters = { "@xrefl/emit_registry.lua" },
--           headers = { "src/**.h" },
--       })
--
-- The same configuration can be given through functions instead:
--
--       add_rules("xrefl")
--       reflect_annotation("REFLECT", { applies_to = "struct" })
--       reflect_emitter("tools/emit_registry.lua")
--       reflect_headers("src/**.h")

add_moduledirs(path.join(os.scriptdir(), "modules"))

local XREFL_ROOT = os.scriptdir()
local CHECKOUT = path.directory(XREFL_ROOT)

-- Plain functions rather than xmake scope APIs, so a table can be serialised
-- into the string list add_values stores. Called inside target(), they apply
-- to that target.

function reflect_annotation(name, options)
    -- Ordered keys, since this string is compared between builds.
    add_values("xrefl.annotations",
               string.serialize({name = name, options = options or {}},
                                {strip = true, indent = false, orderkeys = true}))
end

-- Resolved against the xmake.lua being interpreted.
function reflect_emitter(script)
    add_values("xrefl.emitters", path.absolute(script, os.scriptdir()))
end

function reflect_headers(...)
    add_values("xrefl.headers", ...)
end

-- A macro that expands to nothing, which the parser must skip over.
function reflect_ignore_macro(...)
    add_values("xrefl.ignore_macros", ...)
end

function reflect_inherit(...)
    add_values("xrefl.inherit", ...)
end

function reflect_publish()
    add_values("xrefl.publish", "true")
end

rule("xrefl")
    set_extensions(".h", ".hpp", ".hh", ".hxx")

    -- clangd needs compile_commands.json to find the annotation header.
    add_deps("plugin.compile_commands.autoupdate")

    on_load(function (target)
        import("xrefl.configure")
        import("xrefl.plan")
        target:data_set("xrefl.root", XREFL_ROOT)
        configure.apply(target, target:extraconf("rules", "xrefl"),
                        path.join(CHECKOUT, "emitters"))
        -- The runtime headers the reference emitters generate against.
        target:add("includedirs", path.join(CHECKOUT, "runtime"), {public = true})
        plan.attach(target)
    end)

    before_build(function (target, opt)
        import("xrefl.generate")
        generate.run(target, opt)
    end)
