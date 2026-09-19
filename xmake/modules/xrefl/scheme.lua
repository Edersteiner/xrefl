-- Collects a target's scheme: its annotations, emitters and ignored macros,
-- including anything inherited from a package or dependency.

import("core.base.json")
import("xrefl.order")

local PAYLOAD_DIR = path.join("share", "xrefl")

function _merge_annotations(into, from, origin)
    for name, options in pairs(from) do
        local existing = into[name]
        if existing and existing.origin ~= origin then
            raise("xrefl: annotation '%s' is declared by both %s and %s", name, existing.origin,
                  origin)
        end
        options.origin = origin
        into[name] = options
    end
end

function _load_payload(root, origin)
    local schemefile = path.join(root, "scheme.lua")
    if not os.isfile(schemefile) then
        return nil
    end
    -- import() only exposes functions, so the scheme is returned from one.
    local module = import("scheme", {rootdir = root, anonymous = true})
    if type(module.scheme) ~= "function" then
        raise("xrefl: %s does not define scheme(); regenerate it with a newer xrefl", schemefile)
    end
    local published = module.scheme()
    local emitters = {}
    for _, relative in ipairs(published.emitters or {}) do
        local absolute = path.join(root, relative)
        if not os.isfile(absolute) then
            raise("xrefl: %s lists emitter '%s', which is not installed", origin, relative)
        end
        table.insert(emitters, absolute)
    end
    return {
        annotations = published.annotations or {},
        emitters = emitters,
        ignore_macros = published.ignore_macros or {},
    }
end

function _resolve_inherited(target, name, visiting)
    local pkg = target:pkg(name)
    if pkg and pkg:installdir() then
        local payload = _load_payload(path.join(pkg:installdir(), PAYLOAD_DIR), "package " .. name)
        if payload then
            return payload
        end
    end

    local dep = target:dep(name)
    if dep then
        local scriptdir = dep:scriptdir()
        if scriptdir then
            local payload = _load_payload(path.join(scriptdir, PAYLOAD_DIR), "target " .. name)
            if payload then
                return payload
            end
        end
        -- A dependency in the same project does not need to have published.
        return collect(dep, visiting)
    end

    raise("xrefl: reflect_inherit(\"%s\"): no package or dependency by that name.\n" ..
          "  note: add_packages(\"%s\") or add_deps(\"%s\") before inheriting from it",
          name, name, name)
end

function collect(target, visiting)
    -- Guards against a cycle through reflect_inherit.
    visiting = visiting or {}
    if visiting[target:name()] then
        raise("xrefl: reflect_inherit forms a cycle through target '%s'", target:name())
    end
    visiting[target:name()] = true

    local annotations = {}
    local emitters = {}
    local ignore_macros = {}
    local seen_emitter = {}

    local function add_emitter(script)
        if not seen_emitter[script] then
            seen_emitter[script] = true
            table.insert(emitters, script)
        end
    end

    -- Inherited first, so the target's own declarations come after.
    for _, name in ipairs(target:values("xrefl.inherit") or {}) do
        local inherited = _resolve_inherited(target, name, visiting)
        _merge_annotations(annotations, inherited.annotations, "inherited from " .. name)
        for _, script in ipairs(inherited.emitters) do
            add_emitter(script)
        end
        for _, macro in ipairs(inherited.ignore_macros) do
            table.insert(ignore_macros, macro)
        end
    end

    for _, serialized in ipairs(target:values("xrefl.annotations") or {}) do
        local declaration = string.deserialize(serialized)
        _merge_annotations(annotations, {[declaration.name] = declaration.options},
                           "target " .. target:name())
    end

    for _, script in ipairs(target:values("xrefl.emitters") or {}) do
        if not os.isfile(script) then
            raise("xrefl: emitter '%s' does not exist", script)
        end
        add_emitter(script)
    end

    for _, macro in ipairs(target:values("xrefl.ignore_macros") or {}) do
        table.insert(ignore_macros, macro)
    end

    visiting[target:name()] = nil
    return {
        annotations = annotations,
        emitters = emitters,
        ignore_macros = table.unique(ignore_macros),
    }
end

-- Writes what reflect_inherit reads back: the scheme as a function-returning
-- module, since import() only exposes functions, plus a copy of each emitter.
function publish(target, target_scheme, payloaddir)
    local emitters = {}
    os.mkdir(path.join(payloaddir, "emitters"))
    for _, script in ipairs(target_scheme.emitters) do
        local relative = path.join("emitters", path.filename(script))
        os.cp(script, path.join(payloaddir, relative))
        table.insert(emitters, (relative:gsub("\\", "/")))
    end

    local published = {
        annotations = {},
        emitters = emitters,
        ignore_macros = target_scheme.ignore_macros,
    }
    for name, options in pairs(target_scheme.annotations) do
        local copy = table.clone(options)
        copy.origin = nil
        published.annotations[name] = copy
    end

    local content = ("-- Generated by xrefl for target '%s'. Do not edit.\n" ..
                     "--\n" ..
                     "-- Read back by reflect_inherit(\"%s\") in a consuming target.\n" ..
                     "function scheme()\n    return %s\nend\n")
                    :format(target:name(), target:name(),
                            string.serialize(published, {indent = true, orderkeys = true}))
    io.writefile(path.join(payloaddir, "scheme.lua"), content)
end

function annotation_names(scheme)
    local names = {}
    for name, _ in pairs(scheme.annotations) do
        table.insert(names, name)
    end
    return order.sort(names)
end

-- Fingerprint of everything other than the headers that changes generated
-- output. Dependency tracking keys on it. An emitter may import helpers
-- beside it, so every Lua file in its directory counts as part of it.
function fingerprint(scheme)
    local parts = {}
    for _, name in ipairs(annotation_names(scheme)) do
        table.insert(parts, name .. "=" ..
                     string.serialize(scheme.annotations[name],
                                      {strip = true, indent = false, orderkeys = true}))
    end
    local hashed = {}
    for _, script in ipairs(scheme.emitters) do
        local siblings = os.isfile(script) and os.files(path.join(path.directory(script), "*.lua"))
                         or {script}
        for _, file in ipairs(order.sort(siblings)) do
            if not hashed[file] then
                hashed[file] = true
                table.insert(parts, file .. ":" .. (os.isfile(file) and hash.sha256(file) or "?"))
            end
        end
    end
    for _, macro in ipairs(scheme.ignore_macros) do
        table.insert(parts, "ignore:" .. macro)
    end
    return hash.strhash32(table.concat(parts, "\n"))
end
