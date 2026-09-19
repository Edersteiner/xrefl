-- xmake check [--update] [suite] [files...]. Builds the parser first.

task("check")
    set_category("action")

    on_run(function ()
        import("core.base.option")
        import("core.base.task")
        import("core.project.config")
        import("core.project.project")

        config.load()
        task.run("build", {target = "xrefl"})
        local xrefl = path.absolute(project.target("xrefl"):targetfile(), os.projectdir())

        -- xmake stops reading flags at the first positional, so a trailing
        -- --update lands in `files`.
        local files = {}
        local update = option.get("update") or false
        for _, file in ipairs(option.get("files") or {}) do
            if file == "--update" or file == "-u" then
                update = true
            else
                table.insert(files, file)
            end
        end
        local suite = option.get("suite") or "all"
        local opt = {update = update, files = files}
        local suites = {"parser", "projects", "incremental"}
        if suite ~= "all" then
            suites = {suite}
        end

        local ok = true
        for _, name in ipairs(suites) do
            local module = import(name, {rootdir = path.join(os.scriptdir(), "suites"),
                                         anonymous = true, try = true})
            if not module then
                raise("unknown suite '%s'; one of parser, projects, incremental, corpus", name)
            end
            if not module.main(xrefl, opt) then
                ok = false
            end
            print("")
        end
        if not ok then
            raise("some checks failed")
        end
    end)

    set_menu {
        usage = "xmake check [options] [suite] [files...]",
        description = "Run the test suites.",
        options = {
            {"u", "update", "k", nil, "Rewrite the golden files instead of comparing."},
            {nil, "suite", "v", "all", "parser, projects, incremental, corpus, or all.",
             "  all runs everything except corpus, which needs files."},
            {nil, "files", "vs", nil, "Headers for the corpus suite."},
        }
    }
