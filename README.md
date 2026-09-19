# xrefl

A C++ reflection and code generation plugin for xmake.

Every project that needs reflection ends up writing the same tool. Parse the
headers, find the annotated declarations, write out generated code. The parsing
part is the same everywhere. The output is not. One project wants ECS component
registration, another wants JSON serialization, another wants script bindings.
Tools like moc and UHT solve this by shipping one fixed emitter, which only
helps if you want exactly what they produce.

xrefl splits it differently. The parser is the tool. The emitter is a Lua
script you write yourself. xmake is the host because it already has the
compile flags, dependency tracking, a Lua VM and a package registry.

## Installing

xrefl installs as an xmake package. tree-sitter and the C++ grammar are
vendored as C source, so building it needs a C++ compiler and nothing else.
No LLVM, no npm, no grammar generation step.

Note: Currently waiting on xrepo submission

```lua
add_requires("xrefl")

target("mygame")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("xrefl")

    add_rules("@xrefl/reflect", {
        annotations = {
            REFLECT  = { applies_to = {"struct", "enum"}, args = { category = "string?" } },
            PROPERTY = { applies_to = "field",  args = { range = "table?", transient = "flag" } },
        },
        emitters = { "@xrefl/emit_registry.lua", "tools/emit_custom.lua" },
        headers  = { "src/**.h" },
    })
```

That is all of it. No path to a binary, no environment variable, no checkout.
An emitter named `@xrefl/<name>.lua` is one of the reference emitters that
ship with the package. The runtime headers they generate against are added to
the include path for you.

### From a checkout instead

If you would rather keep xrefl in your own tree, as a submodule or a copy,
`includes()` it. The rule is then called `xrefl` and takes the same table:

```lua
includes("third_party/xrefl/xmake/xrefl.lua")

target("mygame")
    add_rules("xrefl", {
        annotations = {
            REFLECT  = { applies_to = "struct", args = { category = "string?" } },
            PROPERTY = { applies_to = "field",  args = { range = "table?", asset = "string?" } },
        },
        emitters = { "tools/emit_registry.lua" },
        headers  = { "src/**.h" },
    })
```

The parser is found in the checkout's own build directory once you have run
`xmake` there, or through the `XREFL` environment variable, or on `PATH`.

The same configuration can be written as functions, which read better when a
target has a lot to declare:

```lua
target("mygame")
    add_rules("xrefl")

    reflect_annotation("REFLECT", {
        applies_to = "struct",
        args = { category = "string?" },
    })
    reflect_annotation("PROPERTY", {
        applies_to = "field",
        args = { range = "table?", asset = "string?" },
    })

    reflect_emitter("tools/emit_registry.lua")
    reflect_headers("src/**.h")
```

A rule that ships inside a package cannot add functions to xmake's description
scope, which is why the packaged form only takes the table.

| Table | Function | What it does |
| --- | --- | --- |
| `annotations = {...}` | `reflect_annotation(name, opts)` | declares an annotation and its arguments |
| `emitters = {...}` | `reflect_emitter(script)` | adds an emitter, several per target is normal |
| `headers = {...}` | `reflect_headers(patterns...)` | headers to parse, same patterns as `add_files` |
| `ignore_macros = {...}` | `reflect_ignore_macro(names...)` | macros the parser skips over |
| `inherit = {...}` | `reflect_inherit(name)` | uses the annotations and emitters of a package or dependency |
| `publish = true` | `reflect_publish()` | makes this target's annotations and emitters available to inherit |

## Annotating

```cpp
#include <mygame/reflect_annotations.h>   // generated

REFLECT(category = "world")
struct Door {
    PROPERTY() uint8_t state;
    PROPERTY(range = {0.1, 20.0}) float speed;
    float animCursor;                     // not annotated, not reflected
};
```

The generated header defines each annotation as a macro that expands to
nothing. Annotated code compiles unchanged and the parser reads the
annotations from the source text.

`applies_to` takes one kind or a list of them: `"struct"`, `"class"`,
`"union"`, `"enum"`, `"field"`, `"function"`. Argument types are `"string"`,
`"number"`, `"boolean"`, `"table"` or `"any"`, with a `?` on the end for
optional, and `"flag"`. A flag is written bare:

```cpp
PROPERTY(transient, range = {0.1, 20.0}) float speed;
```

`transient` here is `transient = true`, and an emitter reads it as such. A
flag is always optional and takes no value; `transient = 3` is an error.

A misspelled argument is an error, not a silent no-op:

```
src/door.h:10:5: annotation 'PROPERTY' has no argument 'rnage'
  note: it accepts asset, range, transient
```

## Emitters

An emitter is a Lua module with an `emit` function. There is no base class and
no framework.

```lua
function emit(unit, out)
    for _, record in ipairs(unit.structs) do
        if record.annotations.REFLECT then
            out.header:write("#include \"%s\"\n", unit.include_path)
            out:write("static const FieldInfo %s_fields[] = {\n", record.symbol)
            for _, field in ipairs(record.fields) do
                if field.annotations.PROPERTY then
                    out:write("    { \"%s\", offsetof(%s, %s), sizeof(decltype(%s::%s)) },\n",
                              field.name, record.qualified_name, field.name,
                              record.qualified_name, field.name)
                end
            end
            out:write("};\n")
        end
    end
end

-- Optional. Runs once per target with every unit visible, for tables that
-- need all the types at once.
function emit_target(units, out)
end
```

`out` writes the generated source. `out.header` writes a companion header,
which is only created if something is written to it. Each reflected
declaration has `qualified_name` (`game::Door`) and `symbol` (`game_Door`,
usable as a C++ identifier). `unit.include_path` is how the project already
spells an `#include` of that header, worked out from the target's include
directories.

Base classes are recorded as written in the source and then matched against
the target's reflected types before any emitter runs. Working out that
`Entity` inside `namespace game` means `game::Entity` needs every type in the
target, which no single header knows. A matched base gets `resolved` (the
qualified name) and, if it lives in another header, `generated_header`, which
is what generated code has to include to refer to it. An unmatched base keeps
`resolved` as nil. It may well be a type from a third-party library, and an
emitter has nothing to say about it.

There is one generated file per input header no matter how many emitters run,
plus one aggregate per target. This is what makes incremental builds work,
since the output for a header depends on that header alone.

Emitters run in xmake's module sandbox. `pcall`, `select`, `load` and `error`
are not available. `assert`, `type`, `table.pack`, `table.unpack` and
`string.format` are.

Generated output must not depend on the machine that built it. Two things
in Lua do: string order follows the process locale's collation, and
`tostring` on a number follows its decimal point. Headers are ordered by
byte, and `out:write` spells a number argument with a full stop whatever the
locale, so format one with `%s`. `out:number(value)` does the same for a
number you concatenate yourself. `import("xrefl.order")` has the byte-order
comparator for lists of your own.

## Reference emitters

`emitters/` holds working emitters and `runtime/` the headers they generate
against. Both are examples. Copy them into your project and change them. Each
one names the annotations it looks for in a constant at the top, since xrefl
itself has no opinion about what they are called.

**`emit_registry.lua`** produces field tables and type registration. Per type
you get a `FieldInfo` table with names, spelled types, offsets and sizes, plus
a `TypeInfo` that links reflected base classes. Per target you get a registry
to look types up by name.

**`emit_json_yyjson.lua`** produces JSON serialization.

- Reads check whether a key is present rather than requiring it. A missing key
  leaves the member at its default, so adding a field does not invalidate
  files written before it existed. A key that is present but has the wrong
  type is an error.
- Enums serialize as strings by name.
- A field of a type the emitter does not know is written through
  `xrefl::json::write`, which resolves by overloading. Write one pair of
  functions by hand for a leaf type and every type containing it works, at any
  depth, without the emitter knowing anything about it. Declare that pair in
  the header where the type is declared. Generated code sees exactly what the
  annotated header includes and nothing else.
- `PROPERTY(transient)` keeps a field out of the serialized form while
  leaving it visible to every other emitter.
- A derived type's functions call its base's first, so inherited fields end up
  in the same object instead of nested or flattened by hand.
- `xrefl::json::to_string(value)` and `from_string(text, value)` do the
  document handling for you when you have a single value.

**Polymorphic serialization** is what the base class tracking is for. Mark the
root with `REFLECT(polymorphic)`. Types deriving from it are registered
too, without repeating the marking.

```cpp
shapes::Square square;
const shapes::Shape& as_shape = square;

std::string text = xrefl::json::save_polymorphic(xrefl_json_registry(), as_shape);
// {"$type":"shapes::Square","name":"tile","width":3.0,"height":3.0,"locked":true}

auto loaded = xrefl::json::load_polymorphic<shapes::Shape>(xrefl_json_registry(), text);
```

The registry is built in the target phase, the only place that sees every
header at once. Loading checks that the type named in the document really
derives from the one you asked for before it constructs anything, so a
document naming an unrelated type is refused instead of reinterpreted. Each
step of the upcast happens where both types are complete, so a base at a
non-zero offset works. Concrete types must be default constructible, which a
`static_assert` enforces, and a polymorphic base needs a virtual destructor.

Saving through a base reference is the one part that needs RTTI. Nothing else
can recover the derived type without adding a virtual to your own type. Define
`XREFL_JSON_NO_RTTI` to leave it out. Loading does not need it.

`load_polymorphic` takes the registry as an argument rather than using a
global, because a program can link more than one reflected target.

## Inheriting annotations and emitters

A library declares its annotations and emitters once and publishes them:

```lua
target("engine")
    add_rules("xrefl")
    reflect_annotation("REFLECT", { applies_to = "struct" })
    reflect_emitter("tools/emit_names.lua")
    reflect_publish()
```

A consumer picks them up with one line and declares nothing of its own:

```lua
target("mygame")
    add_rules("xrefl")
    add_packages("engine")
    reflect_inherit("engine")
    reflect_headers("src/**.h")
```

This works across an xrepo package boundary as well as between targets in one
project. The library writes its annotations and emitters into `share/xrefl`
and the consumer reads them back from the installed package.

## Incremental builds

Touching a header reparses that header and nothing else. A rebuild with no
changes does no work.

The generated output for a header depends only on that header's parse and the
annotation setup, so the dependency list does not follow `#include`s. Nothing
in another header can change what this one emits. What the generated file
needs at compile time is a different question, and the C++ compiler's own
dependency tracking already handles that.

Generation reruns when any of these change: the header's content, an emitter
script or any Lua file in its directory (an emitter may `import` a helper
beside it), an annotation declaration, the ignored macros, the set of headers
matched, or the parser binary itself. Deleting a generated file by hand brings
it back. Deleting a header removes its output.

Measured on a 500-header project with 6000 reflected fields:

| | |
| --- | --- |
| cold build, including all C++ compilation | 1.9s |
| rebuild with no changes | 0.6s |
| rebuild after editing one header | 0.8s, one header reparsed |

`xmake check incremental` tests both directions of each case: what has to be
regenerated and what has to stay byte for byte the same. Stale output and full
rebuilds on every save are both failures that the build itself will not report.

## compile_commands.json

The rule pulls in xmake's `plugin.compile_commands.autoupdate`, so
`compile_commands.json` is written on every build. clangd needs it to find the
generated annotation header. Without it every annotated declaration shows up
as an error in the editor.

## The parser

```
xrefl parse --annotations REFLECT,PROPERTY,METHOD --out-dir gen src/**.h
```

| Option | Meaning |
| --- | --- |
| `--annotations A,B,C` | annotation names to look for, nothing else counts |
| `--annotation NAME` | add one name, repeatable |
| `--ignore-macros A,B` | macros to blank before parsing, see below |
| `--ignore-macro NAME` | add one macro, repeatable |
| `--out-dir DIR` | write one JSON file per header |
| `--root DIR` | record paths relative to this, keeps output reproducible |
| `--stdout` | write one unit to stdout instead |
| `--pretty` | indent the JSON |

The parser has no opinion about annotation names. It is told which names to
look for and records what it finds.

### How annotations are found

tree-sitter does not run the preprocessor, so it would see `REFLECT()` as
literal tokens and parse them badly in front of a declaration. Instead xrefl
runs a token pass first. It finds every annotation, keeps its argument text as
written, and overwrites the whole thing with spaces in a scratch copy of the
source. Newlines are kept, so every byte offset and line number still points
at the same place in the original file.

tree-sitter then parses ordinary C++ with no annotations in it, and each
annotation is attached to the declaration that starts at the next non-blank
byte. The grammar never sees a macro, so grammar quirks cannot break the
attachment, and diagnostics keep exact `file:line:column` positions.

Annotation arguments are Lua, not C++, and the parser never interprets them:

```cpp
PROPERTY(range = {0.1, 20.0}) float speed;
```

`range = {0.1, 20.0}` is passed through as text. The plugin evaluates it and
checks it against the declared argument types.

### Types are spelled, not resolved

The parser records types as written. It never resolves a typedef, computes a
size or calculates an offset. Generated code writes `offsetof(T, field)` and
`sizeof(decltype(T::field))` and lets the C++ compiler answer. That is why a
syntactic parser is enough.

A type spelling is the declaration's shared prefix joined to one declarator
with the declared name cut out, which gives you the abstract declarator
exactly as it was written:

| Declaration | Recorded type |
| --- | --- |
| `const MeshHandle* mesh` | `const MeshHandle*` |
| `int arr[4]` | `int[4]` |
| `int* pointers[8]` | `int*[8]` |
| `void (*callback)(int, float)` | `void (*)(int, float)` |
| `int a, *b, c[3]` | `int`, `int *`, `int[3]` |

## Macros in front of declarations

Real headers put macros that expand to nothing in front of types. `IMGUI_API`,
`PACKED`, `ALIGNED(16)`, `IM_MSVC_RUNTIME_CHECKS_OFF`. Without the
preprocessor these read as the start of one big declaration that swallows
whatever comes after. xrefl notices and names the macro:

```
imgui.h:2:1: error: the declaration annotated with 'REFLECT' is swallowed by a
declaration that starts at 'IM_MSVC_RUNTIME_CHECKS_OFF' [absorbed-declaration]
  note: if 'IM_MSVC_RUNTIME_CHECKS_OFF' is a macro, declare it with
  reflect_ignore_macro("IM_MSVC_RUNTIME_CHECKS_OFF") so the parser can skip
  over it
```

Listing the macro in `ignore_macros` blanks it the same way annotations are
blanked, and the declaration parses. The compiler still needs the real macro
definition, this only tells the parser to skip it.

## Known limits

A syntactic parser cannot do everything. Every limit fails with a diagnostic
code and a note on what to do instead.

| Code | Cause |
| --- | --- |
| `annotated-template` | an annotation on a template, which is not supported |
| `absorbed-declaration` | a macro in front of the declaration, add it to `ignore_macros` |
| `unparsable-declaration` | a macro in type position, or something the grammar does not model |
| `unsupported-declaration` | parsed, but not a kind of declaration xrefl reflects |
| `unattached-annotation` | no declaration follows the annotation |
| `unbalanced-annotation` | the annotation's parentheses do not close |
| `annotated-forward-declaration` | the annotation is on a declaration with no body |
| `annotated-anonymous-type` | the annotated type has no name |

The parser sits behind the parsed unit interface, so a libclang backend could
be added for projects that need real semantic resolution, and C++26 static
reflection can replace it once compilers support it.

## Building xrefl itself

```
xmake
```

Nothing else is needed. `vendor/README.md` lists what is vendored, and
`xmake vendor` pulls it again at the pinned versions. `packages/x/xrefl/` holds
the package definition and the rule that ships with it.

## Tests

Everything runs through xmake. It builds the parser first, so the tests always
run against the current source.

```
xmake check                    # parser goldens, end-to-end projects, incremental
xmake check --update parser    # rewrite the parser goldens
xmake check --update projects  # rewrite the project expectations
xmake check incremental
xmake check corpus <headers>...
```

`tests/cases/` holds parser inputs and `tests/golden/` holds what each one must
produce, diagnostics included. `tests/projects/` holds real xmake projects.
Each has a `test.conf` that says whether the build should succeed and run, or
fail with a particular diagnostic. The incremental suite checks both what has
to be regenerated after a change and what has to stay the same.

`xmake check corpus` parses each header, inserts an annotation at every
declaration the parser reported, parses again and requires every annotation to
come back attached to the same declaration. Run against ImGui, Box2D, Bullet,
EnTT, glm, nlohmann/json and libstdc++ (767 headers with declarations, 2381
structs, 12251 fields, 7340 methods) it reports no mismatches.

## License

Zero-Clause BSD. Use it, copy it, change it, ship it, no notice required. The
reference emitters and runtime headers are meant to be copied into your
project and you owe nothing for doing so. The vendored tree-sitter sources
keep their own MIT notices under `vendor/`.
