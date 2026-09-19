-- Pass/fail tally and diff output shared by every suite.

function new(title)
    print("== %s ==", title)
    return {title = title, passed = 0, failed = 0, names = {}}
end

function pass(self, name)
    print("ok   %s", name)
    self.passed = self.passed + 1
end

function fail(self, name, detail)
    print("FAIL %s", name)
    if detail and #detail > 0 then
        for line in (detail .. "\n"):gmatch("(.-)\n") do
            print("     %s", line)
        end
    end
    self.failed = self.failed + 1
    table.insert(self.names, name)
end

function finish(self)
    print("")
    print("%d passed, %d failed", self.passed, self.failed)
    if self.failed > 0 then
        print("failed: %s", table.concat(self.names, " "))
    end
    return self.failed == 0
end

function _lines(text)
    local out = {}
    for line in (text .. "\n"):gmatch("(.-)\n") do
        table.insert(out, line)
    end
    if out[#out] == "" then
        table.remove(out)
    end
    return out
end

-- Line diff by longest common subsequence, so an inserted line does not show
-- everything after it as changed. Goldens are small enough for the quadratic
-- table.
function diff(expected, actual)
    local a = _lines(expected)
    local b = _lines(actual)
    local n, m = #a, #b

    local lcs = {}
    for i = n + 1, 1, -1 do
        lcs[i] = {}
        for j = m + 1, 1, -1 do
            if i > n or j > m then
                lcs[i][j] = 0
            elseif a[i] == b[j] then
                lcs[i][j] = lcs[i + 1][j + 1] + 1
            else
                lcs[i][j] = math.max(lcs[i + 1][j], lcs[i][j + 1])
            end
        end
    end

    local out = {}
    local i, j = 1, 1
    while i <= n or j <= m do
        if i <= n and j <= m and a[i] == b[j] then
            i, j = i + 1, j + 1
        elseif j <= m and (i > n or lcs[i][j + 1] >= lcs[i + 1][j]) then
            table.insert(out, "+" .. b[j])
            j = j + 1
        else
            table.insert(out, "-" .. a[i])
            i = i + 1
        end
    end
    return table.concat(out, "\n")
end

-- Strips colour codes and build progress.
function strip_build_noise(text)
    local kept = {}
    for _, line in ipairs(_lines((text:gsub("\27%[[%d;]*m", "")))) do
        local noise = line:startswith("[") or line:startswith("checking for")
                      or line:startswith("  => ") or line == ""
        if not noise then
            table.insert(kept, line)
        end
    end
    return table.concat(kept, "\n")
end

-- Returns stdout followed by stderr and the exit code, without raising.
function capture(program, argv, opt)
    opt = opt or {}
    local outfile = os.tmpfile()
    local errfile = os.tmpfile()
    local code = os.execv(program, argv, {try = true, stdout = outfile, stderr = errfile,
                                          curdir = opt.curdir, envs = opt.envs})
    local text = (io.readfile(outfile) or "") .. (io.readfile(errfile) or "")
    os.tryrm(outfile)
    os.tryrm(errfile)
    return text, code or -1
end
