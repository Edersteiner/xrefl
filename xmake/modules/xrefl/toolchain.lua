-- Finds the parser binary. XREFL overrides the lookup, which the test suites
-- use to point at the freshly built one. A checkout that has been built is
-- found through its own build directory.

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

    local root = target:data("xrefl.root")
    if root then
        local built = path.join(path.directory(root), "build", os.host(), os.arch(), "release",
                                is_host("windows") and "xrefl.exe" or "xrefl")
        if os.isfile(built) then
            return built
        end
    end

    local tool = find_tool("xrefl")
    if tool and tool.program then
        return tool.program
    end

    raise("xrefl: cannot find the xrefl parser.\n" ..
          "  note: add_requires(\"xrefl\", {kind = \"binary\"}) and add_packages(\"xrefl\"),\n" ..
          "  note: or build the checkout you included, or set XREFL to a built binary")
end
