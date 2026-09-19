package("xrefl")
    set_kind("binary")
    set_homepage("https://github.com/Edersteiner/xrefl")
    set_description("A general-purpose C++ reflection and code generation plugin for XMake")
    set_license("0BSD")

    add_urls("https://github.com/Edersteiner/xrefl.git")
    add_versions("0.1.0", "b2355c3008a870622b23c572b44c944cdc196eb2")

    on_install(function (package)
        import("package.tools.xmake").install(package)
    end)

    on_test(function (package)
        os.vrun("xrefl --version")
    end)
