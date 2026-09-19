-- A flag given a value must fail the build.
includes("../../../xmake/xrefl.lua")

target("flag_value")
    set_kind("binary")
    set_languages("cxx17")
    add_rules("xrefl")
    add_files("src/*.cpp")
    add_includedirs("src")

    reflect_annotation("PROPERTY", { applies_to = "field", args = { transient = "flag" } })
    reflect_emitter("tools/emit_nothing.lua")
    reflect_headers("src/*.h")
