-- Uses xrefl only through the installed package.
add_repositories("xrefl-local ../repo")
add_requires("xrefl")

target("app")
    set_kind("binary")
    set_languages("cxx17")
    add_files("src/*.cpp")
    add_includedirs("src")
    add_packages("xrefl")

    add_rules("@xrefl/reflect", {
        annotations = {
            REFLECT  = { applies_to = "struct", args = { category = "string?" } },
            PROPERTY = { applies_to = "field" },
        },
        emitters = { "@xrefl/emit_registry.lua" },
        headers  = { "src/*.h" },
    })
