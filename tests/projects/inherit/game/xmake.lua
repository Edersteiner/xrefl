-- Inherits the library's annotations and emitters through the package and
-- declares none of its own.
includes("../../../../xmake/xrefl.lua")

add_repositories("xrefl-tests ../repo")
add_requires("engine")

target("game")
    set_kind("binary")
    set_languages("cxx17")
    add_rules("xrefl")
    add_files("src/*.cpp")
    add_includedirs("src")
    add_packages("engine")

    reflect_inherit("engine")
    reflect_headers("src/*.h")
