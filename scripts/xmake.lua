-- xmake vendor: re-pulls tree-sitter and the C++ grammar at the versions
-- pinned in scripts/vendor.lua.

task("vendor")
    set_category("action")
    on_run(function ()
        import("vendor", {rootdir = os.scriptdir(), anonymous = true}).main()
    end)
    set_menu {
        usage = "xmake vendor",
        description = "Re-pull the vendored tree-sitter sources at the pinned versions.",
    }
