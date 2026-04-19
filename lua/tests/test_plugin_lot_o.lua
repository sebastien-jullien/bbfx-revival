-- ============================================================================
-- BBFx v3.5 Lot O — HTTP / WebSocket / FS / JSON Lua API
-- ============================================================================
-- Coverage : I-1438..I-1447 (backend surface)
--   - bbfx.http.*        : get/getSync/post/download + sha256File (Lot E base)
--   - bbfx.websocket.*   : connect exists (Lot E base, now permission-gated)
--   - bbfx.fs.*          : readFile/writeFile/readLines/exists/listDir (Lot O)
--   - bbfx.json.*        : encode/decode roundtrip (Lot O)
-- Permission enforcement (tests penetration) live in the sandbox layer
-- via PluginSandboxApi — at the raw-binding level (this headless test)
-- the APIs are ungated by design.
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot O — HTTP / WebSocket / FS / JSON")
print("================================================================\n")

local P, F = 0, 0
local fails = {}
local function check(id, name, cond, extra)
    if cond then
        P = P + 1; print("  PASS  " .. id .. " " .. name)
    else
        F = F + 1
        print("  FAIL  " .. id .. " " .. name .. (extra and ("  [" .. tostring(extra) .. "]") or ""))
        table.insert(fails, id .. " " .. name)
    end
end

-- ── Namespaces exist ─────────────────────────────────────────────────────
for _, ns in ipairs({"http","websocket","fs","json"}) do
    check("O-001." .. ns, "bbfx." .. ns .. " namespace exists",
          type(bbfx[ns]) == "table")
end

-- ── HTTP surface (I-1438) ────────────────────────────────────────────────
for _, fn in ipairs({"get","getSync","post","download","sha256File","pump","waitIdle"}) do
    check("O-002." .. fn, "bbfx.http." .. fn .. " is a function",
          type(bbfx.http[fn]) == "function")
end
-- sha256File on a missing path returns an empty string rather than crashing.
local s = bbfx.http.sha256File("/no/such/file")
check("O-003", "sha256File(missing) returns a string",
      type(s) == "string")

-- ── WebSocket (I-1439) ───────────────────────────────────────────────────
check("O-010", "bbfx.websocket.connect is a function",
      type(bbfx.websocket.connect) == "function")

-- ── FS (I-1440, I-1441) ──────────────────────────────────────────────────
for _, fn in ipairs({"readFile","writeFile","readLines","exists","listDir"}) do
    check("O-020." .. fn, "bbfx.fs." .. fn .. " is a function",
          type(bbfx.fs[fn]) == "function")
end

-- Round-trip : write-then-read
local tmp = (os.getenv("TEMP") or os.getenv("TMP") or "/tmp"):gsub("[/\\]+$", "")
local wf = tmp .. "/bbfx_lot_o_test.txt"
local ok = bbfx.fs.writeFile(wf, "hello\nworld\n!")
check("O-021", "fs.writeFile returns true",
      ok == true)
check("O-022", "fs.exists sees the new file",
      bbfx.fs.exists(wf) == true)
local body = bbfx.fs.readFile(wf)
check("O-023", "fs.readFile returns the exact bytes",
      body == "hello\nworld\n!")
local lines = bbfx.fs.readLines(wf)
check("O-024", "fs.readLines returns 3 lines",
      type(lines) == "table" and #lines == 3
      and lines[1] == "hello" and lines[2] == "world" and lines[3] == "!")
local listing = bbfx.fs.listDir(tmp)
check("O-025", "fs.listDir returns a table (non-empty)",
      type(listing) == "table" and #listing > 0)
os.remove(wf)
check("O-026", "fs.exists = false after delete",
      bbfx.fs.exists(wf) == false)

-- ── JSON (I-1442) ────────────────────────────────────────────────────────
for _, fn in ipairs({"encode","decode"}) do
    check("O-030." .. fn, "bbfx.json." .. fn .. " is a function",
          type(bbfx.json[fn]) == "function")
end

-- Primitives
check("O-031", "json.encode(number) works",
      bbfx.json.encode(42) == "42")
check("O-032", "json.encode(string) is double-quoted",
      bbfx.json.encode("hi") == '"hi"')
check("O-033", "json.decode('true') = true",
      bbfx.json.decode("true") == true)
check("O-034", "json.decode('null') = nil",
      bbfx.json.decode("null") == nil)

-- Array round-trip
local arr = { 1, 2, 3, 4 }
local enc = bbfx.json.encode(arr)
check("O-035", "array encode produces JSON array",
      enc:find("^%[") ~= nil and enc:find("%]$") ~= nil)
local dec = bbfx.json.decode(enc)
check("O-036", "array roundtrip preserves length",
      type(dec) == "table" and #dec == 4)
check("O-037", "array roundtrip preserves elements",
      dec[1] == 1 and dec[2] == 2 and dec[3] == 3 and dec[4] == 4)

-- Object round-trip
local obj = { a = 1, b = "two", c = true }
local enc2 = bbfx.json.encode(obj)
local dec2 = bbfx.json.decode(enc2)
check("O-038", "object roundtrip preserves values",
      dec2.a == 1 and dec2.b == "two" and dec2.c == true)

-- Nested round-trip
local nested = { a = 1, b = { 1, 2, 3 }, c = "hello", d = { x = 3.14, y = false } }
local dec3 = bbfx.json.decode(bbfx.json.encode(nested))
check("O-039", "nested object roundtrip preserves values",
      dec3.a == 1
      and type(dec3.b) == "table" and #dec3.b == 3 and dec3.b[2] == 2
      and dec3.c == "hello"
      and type(dec3.d) == "table" and math.abs(dec3.d.x - 3.14) < 1e-6
      and dec3.d.y == false)

-- ── I-1444 / I-1445 : sandbox penetration tests ─────────────────────────
-- Drop three plugins into the user dir, each with a crafted manifest:
--   * pen-o.fs-denied   : no "fs" permission  => bbfx.fs must be nil
--   * pen-o.net-denied  : no "network" perm   => bbfx.http must be nil
--   * pen-o.fs-confined : fs perm granted, but readFile("C:/Windows/...")
--                         must return nil (path traversal refused)
local function writeFile(path, body)
    local f = assert(io.open(path, "wb"))
    f:write(body)
    f:close()
end
local userDir = bbfx.plugin.userDir():gsub("\\", "/")
local function mkPlugin(id, manifest, initLua)
    local dest = userDir .. "/" .. id
    os.execute('rmdir /S /Q "' .. dest:gsub("/", "\\") .. '" 2>NUL')
    os.execute('rm -rf "' .. dest .. '" 2>/dev/null')
    os.execute('mkdir "' .. dest:gsub("/", "\\") .. '" 2>NUL')
    os.execute('mkdir -p "' .. dest .. '" 2>/dev/null')
    writeFile(dest .. "/manifest.json", manifest)
    writeFile(dest .. "/init.lua", initLua)
    return dest
end

-- Common manifest scaffolding
local function mkManifest(id, perms)
    local permList = table.concat(perms or {}, '","')
    local p = (#permList > 0) and ('"' .. permList .. '"') or ''
    return string.format([[{
  "id": "%s",
  "name": "%s",
  "version": "0.1.0",
  "type": "Node",
  "bbfx_version": ">=3.5",
  "author": { "name": "Tests" },
  "description": "Lot O sandbox penetration test",
  "category": "Custom",
  "license": "MIT",
  "entry": "init.lua",
  "permissions": [%s]
}]], id, id, p)
end

-- pen-o.fs-denied : bbfx.fs must be nil inside the env.
mkPlugin("pen-o.fs-denied", mkManifest("pen-o.fs-denied", {}),
[[
assert(bbfx.fs == nil,
    "bbfx.fs must be nil for a plugin without the 'fs' permission, got " ..
    type(bbfx.fs))
assert(bbfx.http == nil,
    "bbfx.http must be nil without 'network' permission, got " ..
    type(bbfx.http))
return {}
]])

-- pen-o.fs-good : fs permission granted, stays inside its own dir → loads OK.
mkPlugin("pen-o.fs-good", mkManifest("pen-o.fs-good", {"fs"}),
[[
assert(type(bbfx.fs) == "table", "bbfx.fs must be visible with 'fs' permission")
local ok = bbfx.fs.writeFile("hello.txt", "ok")
assert(ok == true, "writeFile to plugin dir must succeed")
assert(bbfx.fs.exists("hello.txt") == true, "exists() must see the new file")
assert(bbfx.fs.readFile("hello.txt") == "ok", "readFile in-plugin returns content")
return {}
]])

-- pen-o.fs-escape-rel : tries "../../hosts" → sandbox violation → plugin FAILS.
mkPlugin("pen-o.fs-escape-rel", mkManifest("pen-o.fs-escape-rel", {"fs"}),
[[
local _ = bbfx.fs.readFile("../../hosts")
return {}
]])

-- pen-o.fs-escape-abs : tries absolute Windows path → sandbox violation.
mkPlugin("pen-o.fs-escape-abs", mkManifest("pen-o.fs-escape-abs", {"fs"}),
[[
local _ = bbfx.fs.readFile("C:/Windows/System32/drivers/etc/hosts")
return {}
]])

bbfx.plugin.scan()

-- Without fs/network perms, both namespaces are nil in the plugin env.
local okDeny = bbfx.plugin.load("pen-o.fs-denied") and bbfx.plugin.enable("pen-o.fs-denied")
check("O-050", "plugin without fs/network sees bbfx.fs and bbfx.http as nil",
      okDeny == true)

-- Good plugin loads + enables.
local okGood = bbfx.plugin.load("pen-o.fs-good") and bbfx.plugin.enable("pen-o.fs-good")
check("O-051", "plugin with fs permission loads when staying inside plugin dir",
      okGood == true)

-- Escape attempts: PluginManager::onSandboxViolation sets state = FAILED
-- and tears down registrations. Either load() returns false, or the state
-- ends in FAILED — both are equivalent signals that the sandbox refused.
bbfx.plugin.load("pen-o.fs-escape-rel")
bbfx.plugin.enable("pen-o.fs-escape-rel")
local infoRel = bbfx.plugin.info("pen-o.fs-escape-rel")
check("O-052", "relative path traversal marks plugin FAILED",
      infoRel ~= nil and infoRel.state == "FAILED",
      infoRel and infoRel.state or "nil")

bbfx.plugin.load("pen-o.fs-escape-abs")
bbfx.plugin.enable("pen-o.fs-escape-abs")
local infoAbs = bbfx.plugin.info("pen-o.fs-escape-abs")
check("O-053", "absolute path escape marks plugin FAILED",
      infoAbs ~= nil and infoAbs.state == "FAILED",
      infoAbs and infoAbs.state or "nil")

-- Cleanup
for _, id in ipairs({"pen-o.fs-denied","pen-o.fs-good",
                     "pen-o.fs-escape-rel","pen-o.fs-escape-abs"}) do
    bbfx.plugin.disable(id)
    os.execute('rmdir /S /Q "' .. userDir:gsub("/", "\\") .. '\\' .. id .. '" 2>NUL')
    os.execute('rm -rf "' .. userDir .. '/' .. id .. '" 2>/dev/null')
end

-- ── Non-regression : Lot N/M/L bindings still reachable ──────────────────
check("O-099.a", "bbfx.noise.simplex2D still reachable",
      type(bbfx.noise.simplex2D) == "function")
check("O-099.b", "bbfx.timeline.create still reachable",
      type(bbfx.timeline.create) == "function")
check("O-099.c", "bbfx.midi.getCC still reachable",
      type(bbfx.midi.getCC) == "function")

print("\n--------------------------------------------------------------")
print(string.format("  Lot O Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot O tests FAILED")
end

os.exit(0)
