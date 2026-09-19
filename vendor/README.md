# Vendored dependencies

Committed as source so that xrefl builds with nothing but a C++ compiler. No
npm, no tree-sitter CLI, no grammar generation step.

| Source | Version | Upstream |
| --- | --- | --- |
| `tree-sitter` | v0.27.0 | https://github.com/tree-sitter/tree-sitter |
| `tree-sitter-cpp` | v0.23.4 | https://github.com/tree-sitter/tree-sitter-cpp |

The grammar's `LANGUAGE_VERSION` is 14. The runtime accepts 13 through 15, so
the two versions above are compatible. Check this again after any bump.

`vendor/tree-sitter/src/wasm-stdlib` is left out. It is only needed with
`TREE_SITTER_FEATURE_WASM`, which xrefl does not define.

Run `xmake vendor` to re-pull both at the versions pinned in `scripts/vendor.lua`.
