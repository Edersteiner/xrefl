includes("../../../xmake/xrefl.lua")

add_requires("yyjson")

target("json")
    set_kind("binary")
    set_languages("cxx17")
    add_rules("xrefl")
    add_files("src/*.cpp")
    add_includedirs("src", "../../../runtime")
    add_packages("yyjson")

    reflect_annotation("REFLECT", {
        applies_to = {"struct", "enum"},
        args = { polymorphic = "flag" },
    })
    reflect_annotation("PROPERTY", { applies_to = "field", args = { transient = "flag" } })
    reflect_emitter("../../../emitters/emit_json_yyjson.lua")
    reflect_headers("src/*.h")
