package("engine")
    set_sourcedir(path.join(os.scriptdir(), "..", "..", "..", "..", "engine"))

    on_install(function (package)
        import("package.tools.xmake").install(package)
    end)
