-- A library that publishes its annotations and emitters.
includes("../../../../xmake/xrefl.lua")

target("engine")
    set_kind("static")
    set_languages("cxx17")
    add_rules("xrefl")
    add_files("src/*.cpp")
    add_includedirs("src", {public = true})
    add_headerfiles("src/(engine/*.h)")

    reflect_annotation("REFLECT", { applies_to = "struct", args = { category = "string?" } })
    reflect_annotation("PROPERTY", { applies_to = "field", args = { asset = "string?" } })
    reflect_ignore_macro("ENGINE_API")
    reflect_emitter("tools/emit_names.lua")
    reflect_headers("src/engine/*.h")
    reflect_publish()
