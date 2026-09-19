target("subdir")
    set_kind("binary")
    set_languages("cxx17")
    add_files("src/*.cpp")
    add_includedirs("include")

    add_rules("xrefl", {
        annotations = {
            REFLECT  = { applies_to = "struct", args = { category = "string?" } },
            PROPERTY = { applies_to = "field", args = { range = "table?", transient = "flag" } },
        },
        emitters = { "tools/emit_fields.lua" },
        headers  = { "include/**.h" },
    })
