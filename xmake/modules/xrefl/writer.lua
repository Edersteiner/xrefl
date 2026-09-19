-- The `out` object an emitter writes through. `out` is the generated source,
-- `out.header` a companion header that only exists if written to.

-- What this process spells a decimal point as. Lua formats numbers through
-- the C library, which follows the locale xmake inherited, and generated
-- code needs a full stop whatever the machine's language.
local POINT = tostring(0.5):match("^0(.)5$") or "."

-- A number spelled as C++ reads it, in every locale.
function number(value)
    local text = tostring(value)
    if POINT ~= "." then
        text = text:gsub(POINT, ".", 1)
    end
    return text
end

-- Only formats when given arguments, so a literal `%` passes through. A
-- number argument is spelled through number(), so `%s` is safe for one.
function _write(self, text, ...)
    local args = table.pack(...)
    if args.n > 0 then
        for i = 1, args.n do
            if type(args[i]) == "number" then
                args[i] = number(args[i])
            end
        end
        text = string.format(text, table.unpack(args, 1, args.n))
    end
    table.insert(self._chunks, text)
    return self
end

function _content(self)
    return table.concat(self._chunks)
end

function _used(self)
    return #self._chunks > 0
end

function _buffer()
    return {_chunks = {}, write = _write, content = _content, used = _used,
            number = function (_, value) return number(value) end}
end

function new()
    local out = _buffer()
    out.header = _buffer()
    out.source = out
    return out
end
