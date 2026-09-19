import("devel.git")

local TREE_SITTER_VERSION = "v0.27.0"
local TREE_SITTER_CPP_VERSION = "v0.23.4"

function main()
    local root = os.projectdir()
    local tmp = os.tmpfile() .. ".vendor"
    os.mkdir(tmp)

    git.clone("https://github.com/tree-sitter/tree-sitter.git",
              {depth = 1, branch = TREE_SITTER_VERSION, outputdir = path.join(tmp, "ts")})
    git.clone("https://github.com/tree-sitter/tree-sitter-cpp.git",
              {depth = 1, branch = TREE_SITTER_CPP_VERSION, outputdir = path.join(tmp, "tscpp")})

    local ts = path.join(root, "vendor", "tree-sitter")
    local tscpp = path.join(root, "vendor", "tree-sitter-cpp")
    os.tryrm(ts)
    os.tryrm(tscpp)
    os.mkdir(ts)
    os.mkdir(path.join(tscpp, "src"))

    os.cp(path.join(tmp, "ts", "lib", "include"), ts)
    os.cp(path.join(tmp, "ts", "lib", "src"), ts)
    os.cp(path.join(tmp, "ts", "LICENSE"), ts)
    -- Only needed with TREE_SITTER_FEATURE_WASM.
    os.tryrm(path.join(ts, "src", "wasm-stdlib"))

    os.cp(path.join(tmp, "tscpp", "src", "parser.c"), path.join(tscpp, "src"))
    os.cp(path.join(tmp, "tscpp", "src", "scanner.c"), path.join(tscpp, "src"))
    os.cp(path.join(tmp, "tscpp", "src", "tree_sitter"), path.join(tscpp, "src"))
    os.cp(path.join(tmp, "tscpp", "LICENSE"), tscpp)

    os.tryrm(tmp)
    print("vendored tree-sitter %s, tree-sitter-cpp %s", TREE_SITTER_VERSION,
          TREE_SITTER_CPP_VERSION)
end
