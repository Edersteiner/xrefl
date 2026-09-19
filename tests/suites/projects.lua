-- Builds each directory holding a test.conf. mode=run must build, run and
-- match expected.txt. mode=error must fail the build with that output.

import("report")

-- Both curdir and -P are needed. Relative paths resolve against curdir, and
-- without -P xmake finds this repository's xmake.lua above the project.
function _xmake(project, argv)
    local full = table.join({argv[1], "-P", "."}, table.slice(argv, 2))
    return report.capture(os.programfile(), full, {curdir = project, envs = _envs})
end

function _check(tally, project, mode, opt)
    local name = path.relative(project, path.join(os.projectdir(), "tests", "projects"))
    local expected_file = path.join(project, "expected.txt")

    os.tryrm(path.join(project, "build"))
    os.tryrm(path.join(project, ".xmake"))

    local output, code
    if mode == "run" then
        local text
        text, code = _xmake(project, {"f", "-y"})
        output = text
        if code == 0 then
            text, code = _xmake(project, {"build"})
            output = text
        end
        if code == 0 then
            text, code = _xmake(project, {"run"})
            output = text
        end
    else
        local text
        text, code = _xmake(project, {"f", "-y"})
        output = text
        if code == 0 then
            text, code = _xmake(project, {"build"})
            output = output .. text
        end
    end
    output = report.strip_build_noise(output) .. "\n"

    if opt.update then
        io.writefile(expected_file, output)
        print("updated %s", name)
        return
    end

    if mode == "error" and code == 0 then
        report.fail(tally, name, "expected the build to fail, it succeeded")
        return
    end
    if mode == "run" and code ~= 0 then
        report.fail(tally, name, "build or run failed\n" .. output)
        return
    end
    if not os.isfile(expected_file) then
        report.fail(tally, name, "no expected.txt; run with --update")
        return
    end

    local expected = io.readfile(expected_file)
    if expected == output then
        report.pass(tally, name)
    else
        report.fail(tally, name, report.diff(expected, output))
    end
end

-- A locale that writes a decimal comma and collates past punctuation, when
-- the machine has one. Generated output must not change under it.
function _comma_locale()
    local listing = try { function () return os.iorun("locale -a") end }
    if not listing then
        return nil
    end
    local installed = {}
    for name in listing:gmatch("[^\r\n]+") do
        installed[name:lower():gsub("-", "")] = name
    end
    for _, want in ipairs({"de_de.utf8", "sv_se.utf8", "en_se.utf8", "fr_fr.utf8"}) do
        if installed[want] then
            return installed[want]
        end
    end
    return nil
end

function main(xrefl, opt)
    _envs = {XREFL = xrefl}
    local locale = _comma_locale()
    if locale then
        _envs.LC_ALL = locale
        print("locale: %s", locale)
    end

    -- The packaged test's repo gets the shipped rule copied in, so there is
    -- one copy to maintain.
    local rules = path.join(os.projectdir(), "packages", "x", "xrefl", "rules")
    local testpkg = path.join(os.projectdir(), "tests", "projects", "packaged", "repo",
                              "packages", "x", "xrefl")
    os.tryrm(path.join(testpkg, "rules"))
    os.cp(rules, testpkg)

    local tally = report.new("projects")
    local confs = os.files(path.join(os.projectdir(), "tests", "projects", "**", "test.conf"))
    table.sort(confs)
    for _, conf in ipairs(confs) do
        local mode = (io.readfile(conf) or ""):match("mode=(%w+)") or "run"
        _check(tally, path.directory(conf), mode, opt)
    end
    return opt.update or report.finish(tally)
end
