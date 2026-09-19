-- The basic project, configured through the table the packaged rule takes.
includes("../../../xmake/xrefl.lua")

target("table")
    set_kind("binary")
    set_languages("cxx17")
    add_files("src/*.cpp")
    add_includedirs("src")

    add_rules("xrefl", {
        annotations = {
            REFLECT  = { applies_to = "struct", args = { category = "string?" } },
            PROPERTY = { applies_to = "field",
                         args = { range = "table?", asset = "string?", transient = "flag" } },
        },
        emitters = { "tools/emit_fields.lua" },
        headers  = { "src/*.h" },
    })
