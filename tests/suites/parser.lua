-- Golden tests for the parser. Each cases/<name>.h is compared against
-- golden/<name>.txt, output and exit code included. cases/<name>.args adds
-- flags.

import("report")

local ANNOTATIONS = "REFLECT,PROPERTY,METHOD,ENUM,FUNCTION,SERIALIZE"

function _run_case(xrefl, cases, case)
    local argv = {"parse", "--annotations", ANNOTATIONS}
    local argsfile = path.join(cases, path.basename(case) .. ".args")
    if os.isfile(argsfile) then
        for flag in io.readfile(argsfile):gmatch("%S+") do
            table.insert(argv, flag)
        end
    end
    table.join2(argv, {"--stdout", "--root", cases, case})

    local output, code = report.capture(xrefl, argv)
    output = output:gsub("\n+$", "")
    return output .. "\n--- exit: " .. code .. " ---\n"
end

function main(xrefl, opt)
    local testsdir = path.join(os.projectdir(), "tests")
    local cases = path.join(testsdir, "cases")
    local golden = path.join(testsdir, "golden")
    os.mkdir(golden)

    local tally = report.new("parser")
    local files = os.files(path.join(cases, "*.h"))
    table.sort(files)
    for _, case in ipairs(files) do
        local name = path.basename(case)
        local expected_file = path.join(golden, name .. ".txt")
        local actual = _run_case(xrefl, cases, case)

        if opt.update then
            io.writefile(expected_file, actual)
            print("updated %s", name)
        elseif not os.isfile(expected_file) then
            report.fail(tally, name, "no golden file; run with --update")
        else
            local expected = io.readfile(expected_file)
            if expected == actual then
                report.pass(tally, name)
            else
                report.fail(tally, name, report.diff(expected, actual))
            end
        end
    end
    return opt.update or report.finish(tally)
end
