set_project("xrefl")
local version = "0.1.0"
set_version(version)
set_languages("c11", "cxx17")
set_warnings("all")

add_rules("mode.debug", "mode.release")

-- Development tasks: `xmake check` and `xmake vendor`.
includes("tests", "scripts")

-- `lib.c` includes every other runtime source.
target("tree-sitter")
    set_kind("static")
    set_warnings("none")
    -- Not shipped, only the parser binary is.
    on_install(function (target) end)
    add_files("vendor/tree-sitter/src/lib.c")
    add_includedirs("vendor/tree-sitter/include", {public = true})
    -- Strict c11 hides fdopen and the endian conversions on glibc.
    add_defines("_DEFAULT_SOURCE")

-- Separate target because the grammar ships its own `tree_sitter/parser.h`,
-- which must not shadow the runtime's.
target("tree-sitter-cpp")
    set_kind("static")
    set_warnings("none")
    on_install(function (target) end)
    add_files("vendor/tree-sitter-cpp/src/parser.c")
    add_files("vendor/tree-sitter-cpp/src/scanner.c")

target("xrefl")
    set_kind("binary")
    add_files("src/*.cpp")
    add_deps("tree-sitter", "tree-sitter-cpp")
    add_includedirs("src")
    add_defines('XREFL_VERSION="' .. version .. '"')

    -- What the installed package carries besides the binary.
    add_installfiles("runtime/(xrefl/*.h)", {prefixdir = "include"})
    add_installfiles("xmake/modules/(xrefl/*.lua)", {prefixdir = "share/xrefl/modules"})
    add_installfiles("xmake/xrefl.lua", {prefixdir = "share/xrefl/xmake"})
    add_installfiles("emitters/*.lua", {prefixdir = "share/xrefl/emitters"})
