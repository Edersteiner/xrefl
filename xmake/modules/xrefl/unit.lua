-- Turns the parser's JSON into the unit emitters receive. Annotation
-- arguments arrive as text, are evaluated as Lua here and checked against the
-- declared schema.

import("core.base.json")
import("xrefl.order")

-- Splits the argument text at the commas between arguments: the ones at
-- depth zero outside strings. Nothing inside `{}`, `()` or quotes counts.
function _split_arguments(text)
    local items = {}
    local depth = 0
    local quote = nil
    local start = 1
    local i = 1
    while i <= #text do
        local c = text:sub(i, i)
        if quote then
            if c == "\\" then
                i = i + 1
            elseif c == quote then
                quote = nil
            end
        elseif c == '"' or c == "'" then
            quote = c
        elseif c == "{" or c == "(" or c == "[" then
            depth = depth + 1
        elseif c == "}" or c == ")" or c == "]" then
            depth = depth - 1
        elseif c == "," and depth == 0 then
            table.insert(items, text:sub(start, i - 1))
            start = i + 1
        end
        i = i + 1
    end
    table.insert(items, text:sub(start))
    return items
end

-- A bare name is a flag: `PROPERTY(transient)` reads as `transient = true`.
function _expand_flags(text)
    local items = _split_arguments(text)
    for i, item in ipairs(items) do
        local name = item:match("^%s*([%a_][%w_]*)%s*$")
        if name and name ~= "true" and name ~= "false" and name ~= "nil" then
            items[i] = name .. " = true"
        end
    end
    return table.concat(items, ",")
end

-- string.deserialize evaluates in a restricted environment, so an annotation
-- is data and cannot reach anything.
function _evaluate(text, where)
    local value, errors = ("{" .. _expand_flags(text) .. "\n}"):deserialize()
    if errors then
        raise("xrefl: %s: cannot read annotation arguments (%s)\n  note: %s", where, text,
              errors)
    end
    return value or {}
end

-- A flag is present or absent. Written bare or as `name = true`, never with
-- another value.
function _check_type(value, typespec)
    if typespec == "flag" then
        return value == nil or value == true, "flag"
    end
    local optional = typespec:sub(-1) == "?"
    local wanted = optional and typespec:sub(1, -2) or typespec
    if value == nil then
        return optional, wanted
    end
    if wanted == "any" then
        return true, wanted
    end
    return type(value) == wanted, wanted
end

-- An undeclared argument is an error, otherwise a typo would silently do
-- nothing.
function _validate(name, args, declaration, where)
    local schema = declaration and declaration.args
    if not schema then
        for _ in pairs(args) do
            raise("xrefl: %s: annotation '%s' takes no arguments", where, name)
        end
        return args
    end

    for key, value in pairs(args) do
        if type(key) ~= "string" then
            raise("xrefl: %s: annotation '%s' takes named arguments only", where, name)
        end
        if schema[key] == nil then
            local known = {}
            for declared in pairs(schema) do
                table.insert(known, declared)
            end
            order.sort(known)
            raise("xrefl: %s: annotation '%s' has no argument '%s'\n  note: it accepts %s",
                  where, name, key,
                  #known > 0 and table.concat(known, ", ") or "no arguments")
        end
        local ok, wanted = _check_type(value, schema[key])
        if not ok and wanted == "flag" then
            raise("xrefl: %s: annotation '%s' argument '%s' is a flag, it takes no value\n" ..
                  "  note: write it bare, as in %s(%s)", where, name, key, name, key)
        elseif not ok then
            raise("xrefl: %s: annotation '%s' argument '%s' must be a %s, got a %s",
                  where, name, key, wanted, type(value))
        end
    end

    for key, typespec in pairs(schema) do
        local ok, wanted = _check_type(args[key], typespec)
        if not ok then
            raise("xrefl: %s: annotation '%s' is missing required argument '%s' (%s)",
                  where, name, key, wanted)
        end
    end
    return args
end

-- `symbol` is the qualified name as a C++ identifier. Every emitter needs it.
function _named(item)
    item.qualified_name = item.namespace ~= "" and (item.namespace .. "::" .. item.name)
                          or item.name
    item.symbol = (item.qualified_name:gsub("::", "_"))
    return item
end

function _annotations(list, scheme, unitpath, kind)
    local out = {}
    for _, entry in ipairs(list or {}) do
        local where = ("%s:%d:%d"):format(unitpath, entry.line, entry.column)
        local declaration = scheme.annotations[entry.name]
        if declaration and declaration.applies_to then
            local allowed = declaration.applies_to
            if type(allowed) == "string" then
                allowed = {allowed}
            end
            local ok = false
            for _, candidate in ipairs(allowed) do
                if candidate == kind then
                    ok = true
                    break
                end
            end
            if not ok then
                raise("xrefl: %s: annotation '%s' applies to %s, not to %s %s",
                      where, entry.name, table.concat(allowed, " or "),
                      kind:match("^[aeiou]") and "an" or "a", kind)
            end
        end
        if out[entry.name] then
            raise("xrefl: %s: annotation '%s' is applied twice to the same declaration",
                  where, entry.name)
        end
        local args = _evaluate(entry.args, where)
        out[entry.name] = _validate(entry.name, args, declaration, where)
        out[entry.name].line = nil
    end
    return out
end

function load_json(jsonfile, scheme)
    local raw = json.loadfile(jsonfile)
    local unit = {
        path = raw.path,
        includes = raw.includes or {},
        structs = {},
        enums = {},
        functions = {},
    }

    for _, record in ipairs(raw.structs or {}) do
        local out = {
            name = record.name,
            namespace = record.namespace,
            kind = record.kind,
            annotations = _annotations(record.annotations, scheme, raw.path, record.kind),
            bases = record.bases or {},
            fields = {},
            methods = {},
            line = record.line,
            column = record.column,
        }
        _named(out)
        for _, field in ipairs(record.fields or {}) do
            field.annotations = _annotations(field.annotations, scheme, raw.path, "field")
            table.insert(out.fields, field)
        end
        for _, method in ipairs(record.methods or {}) do
            method.annotations = _annotations(method.annotations, scheme, raw.path, "function")
            table.insert(out.methods, method)
        end
        table.insert(unit.structs, out)
    end

    for _, item in ipairs(raw.enums or {}) do
        item.annotations = _annotations(item.annotations, scheme, raw.path, "enum")
        _named(item)
        table.insert(unit.enums, item)
    end

    for _, item in ipairs(raw.functions or {}) do
        item.annotations = _annotations(item.annotations, scheme, raw.path, "function")
        _named(item)
        table.insert(unit.functions, item)
    end

    return unit
end
