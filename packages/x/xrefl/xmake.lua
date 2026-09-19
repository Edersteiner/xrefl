package("xrefl")
    set_kind("binary")
    set_homepage("https://github.com/Edersteiner/xrefl")
    set_description("A general-purpose C++ reflection and code generation plugin for XMake")
    set_license("0BSD")

    add_urls("https://github.com/Edersteiner/xrefl.git")

    on_install(function (package)
        import("package.tools.xmake").install(package)
    end)

    on_test(function (package)
        os.vrun("xrefl --version")
    end)
