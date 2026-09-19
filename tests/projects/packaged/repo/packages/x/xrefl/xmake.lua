package("xrefl")
    set_kind("binary")
    set_homepage("https://github.com/Edersteiner/xrefl")
    set_description("A general-purpose C++ reflection and code generation plugin for XMake")
    set_license("0BSD")

    -- The test builds the working tree rather than a release.
    set_sourcedir(path.join(os.scriptdir(), "..", "..", "..", "..", "..", "..", ".."))

    on_install(function (package)
        import("package.tools.xmake")
        -- Building the repository in place must not write into its .xmake.
        local envs = xmake.buildenvs(package)
        envs.XMAKE_CONFIGDIR = path.join(package:buildir(), ".xmake")
        xmake.install(package, {}, {envs = envs})
    end)

    on_test(function (package)
        os.vrun("xrefl --version")
    end)
