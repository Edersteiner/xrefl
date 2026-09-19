-- Checks that a change regenerates exactly what depends on it. Each case
-- asserts what must change and what must not.

import("report")

local EMITTER = [[
function emit(unit, out)
    local any = false
    for _, record in ipairs(unit.structs) do
        if record.annotations.REFLECT then
            any = true
            out.header:write("// declares %s\n", record.qualified_name)
            out:write("// %s\n", record.qualified_name)
            for _, field in ipairs(record.fields) do
                if field.annotations.PROPERTY then
                    out:write("//   %s %s\n", field.type, field.name)
                end
            end
        end
    end
    if any then
        out.header:write("#pragma once\n")
    end
end
]]

function _header(name, fields)
    return ("#pragma once\n#include <incr/reflect_annotations.h>\nREFLECT() struct %s { %s };\n")
        :format(name, fields)
end

function _setup(work)
    os.tryrm(work)
    os.mkdir(path.join(work, "src"))
    os.mkdir(path.join(work, "tools"))
    io.writefile(path.join(work, "xmake.lua"), ([[
includes("%s")

target("incr")
    set_kind("binary")
    set_languages("cxx17")
    add_rules("xrefl")
    add_files("src/*.cpp")
    add_includedirs("src")
    reflect_annotation("REFLECT", { applies_to = "struct" })
    reflect_annotation("PROPERTY", { applies_to = "field" })
    reflect_emitter("tools/emit.lua")
    reflect_headers("src/*.h")
]]):format(path.join(os.projectdir(), "xmake", "xrefl.lua")))
    io.writefile(path.join(work, "tools", "emit.lua"), EMITTER)
    io.writefile(path.join(work, "src", "alpha.h"), _header("Alpha", "PROPERTY() int a;"))
    io.writefile(path.join(work, "src", "beta.h"), _header("Beta", "PROPERTY() float b;"))
    io.writefile(path.join(work, "src", "main.cpp"), "int main() { return 0; }\n")
    report.capture(os.programfile(), {"f", "-P", ".", "-y"}, {curdir = work, envs = _envs})
end

-- Headers reparsed by this build, or -1 if it failed.
function _build(work)
    local output, code = report.capture(os.programfile(), {"build", "-P", "."},
                                        {curdir = work, envs = _envs})
    if code ~= 0 then
        print(output)
        return -1
    end
    local count = output:gsub("\27%[[%d;]*m", ""):match("reflecting (%d+) header")
    return tonumber(count) or 0
end

function _gendir(work)
    return os.files(path.join(work, "build", ".gens", "incr", "**", "rules", "xrefl",
                              "incr.xrefl.all.cpp"))[1]:gsub("[/\\]incr%.xrefl%.all%.cpp$", "")
end

function _snapshot(gendir)
    local out = {}
    for _, file in ipairs(table.join(os.files(path.join(gendir, "*.xrefl.cpp")),
                                     os.files(path.join(gendir, "*.xrefl.h")))) do
        out[path.filename(file)] = hash.md5(file)
    end
    return out
end

-- Some filesystems only keep mtime to the second.
function _settle()
    os.sleep(1100)
end

function _check(tally, what, expected, actual)
    if expected == actual then
        report.pass(tally, what)
    else
        report.fail(tally, what, ("expected: %s\nactual:   %s"):format(tostring(expected),
                                                                        tostring(actual)))
    end
end

function main(xrefl, opt)
    _envs = {XREFL = xrefl}
    local work = path.join(os.tmpdir(), "xrefl-incremental")
    local tally = report.new("incremental")

    _setup(work)
    _build(work)
    local gendir = _gendir(work)
    local src = path.join(work, "src")

    print("-- no change --")
    _check(tally, "rebuild reparses nothing", 0, _build(work))

    print("-- touch one header --")
    local before = _snapshot(gendir)
    _settle()
    -- Without an explicit mtime os.touch leaves it alone.
    os.touch(path.join(src, "alpha.h"), {mtime = os.time()})
    _check(tally, "only the touched header is reparsed", 1, _build(work))
    local same = true
    for name, digest in pairs(_snapshot(gendir)) do
        if before[name] ~= digest then
            same = false
        end
    end
    _check(tally, "output is unchanged when content is unchanged", true, same)

    print("-- edit one header --")
    _settle()
    io.writefile(path.join(src, "alpha.h"),
                 _header("Alpha", "PROPERTY() int a; PROPERTY() double added;"))
    _check(tally, "only the edited header is reparsed", 1, _build(work))
    _check(tally, "the edited header's output changed", true,
           io.readfile(path.join(gendir, "src.alpha.h.xrefl.cpp")):find("added", 1, true) ~= nil)
    _check(tally, "the untouched header's output is byte-identical",
           before["src.beta.h.xrefl.cpp"], hash.md5(path.join(gendir, "src.beta.h.xrefl.cpp")))

    print("-- edit the emitter --")
    _settle()
    io.writefile(path.join(work, "tools", "emit.lua"), EMITTER .. "-- changes only the hash\n")
    _check(tally, "every header is reparsed when an emitter changes", 2, _build(work))

    print("-- declare a new annotation --")
    _settle()
    local project = io.readfile(path.join(work, "xmake.lua"))
    io.writefile(path.join(work, "xmake.lua"),
                 (project:gsub("    reflect_emitter",
                               '    reflect_annotation("METHOD", { applies_to = "function" })\n' ..
                               "    reflect_emitter", 1)))
    _check(tally, "every header is reparsed when the vocabulary changes", 2, _build(work))
    _check(tally, "the macro header gained the new annotation", true,
           io.readfile(path.join(gendir, "include", "incr", "reflect_annotations.h"))
               :find("define METHOD", 1, true) ~= nil)

    print("-- delete a generated file --")
    _settle()
    os.rm(path.join(gendir, "src.beta.h.xrefl.cpp"))
    _check(tally, "a deleted output is regenerated", 1, _build(work))
    _check(tally, "the deleted output is back", true,
           os.isfile(path.join(gendir, "src.beta.h.xrefl.cpp")))

    print("-- add a header --")
    _settle()
    io.writefile(path.join(src, "gamma.h"), _header("Gamma", "PROPERTY() char g;"))
    _check(tally, "a new header is parsed on its own", 1, _build(work))
    _check(tally, "the aggregate picked up the new header", true,
           io.readfile(path.join(gendir, "incr.xrefl.all.h")):find("src.gamma.h.xrefl.h", 1, true)
               ~= nil)

    print("-- remove a header --")
    _settle()
    os.rm(path.join(src, "gamma.h"))
    _build(work)
    _check(tally, "the removed header's output is cleaned up", false,
           os.isfile(path.join(gendir, "src.gamma.h.xrefl.cpp")))
    _check(tally, "the aggregate dropped the removed header", false,
           io.readfile(path.join(gendir, "incr.xrefl.all.h")):find("src.gamma.h.xrefl.h", 1, true)
               ~= nil)

    os.tryrm(work)
    return report.finish(tally)
end
