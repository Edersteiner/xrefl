-- Uses the reference registry emitter as shipped.
includes("../../../xmake/xrefl.lua")

target("registry")
    set_kind("binary")
    set_languages("cxx17")
    add_rules("xrefl")
    add_files("src/*.cpp")
    add_includedirs("src", "../../../runtime")

    reflect_annotation("REFLECT", { applies_to = "struct" })
    reflect_annotation("PROPERTY", { applies_to = "field" })
    reflect_emitter("../../../emitters/emit_registry.lua")
    reflect_headers("src/*.h")
