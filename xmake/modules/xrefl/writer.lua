-- The `out` object an emitter writes through. `out` is the generated source,
-- `out.header` a companion header that only exists if written to.

-- Only formats when given arguments, so a literal `%` passes through.
function _write(self, text, ...)
    local args = table.pack(...)
    if args.n > 0 then
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
    return {_chunks = {}, write = _write, content = _content, used = _used}
end

function new()
    local out = _buffer()
    out.header = _buffer()
    out.source = out
    return out
end
