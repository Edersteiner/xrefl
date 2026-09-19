includes("../../../xmake/xrefl.lua")

target("basic")
    set_kind("binary")
    set_languages("cxx17")
    add_rules("xrefl")
    add_files("src/*.cpp")
    add_includedirs("src")

    reflect_annotation("REFLECT", {
        applies_to = "struct",
        args = { category = "string?" },
    })
    reflect_annotation("PROPERTY", {
        applies_to = "field",
        args = { range = "table?", asset = "string?", transient = "boolean?" },
    })

    reflect_emitter("tools/emit_fields.lua")
    reflect_headers("src/*.h")
