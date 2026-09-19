-- Finds the parser binary. XREFL overrides the lookup, which the test suites
-- use to point at the freshly built one.

import("lib.detect.find_tool")

function find(target)
    local override = os.getenv("XREFL")
    if override and os.isfile(override) then
        return override
    end

    local pkg = target:pkg("xrefl")
    if pkg and pkg:installdir() then
        local candidate = path.join(pkg:installdir(), "bin",
                                    is_host("windows") and "xrefl.exe" or "xrefl")
        if os.isfile(candidate) then
            return candidate
        end
    end

    local tool = find_tool("xrefl")
    if tool and tool.program then
        return tool.program
    end

    raise("xrefl: cannot find the xrefl parser.\n" ..
          "  note: add_requires(\"xrefl\", {kind = \"binary\"}) and add_packages(\"xrefl\"),\n" ..
          "  note: or set the XREFL environment variable to a built binary")
end
