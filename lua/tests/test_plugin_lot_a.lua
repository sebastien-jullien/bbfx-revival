-- ============================================================================
-- BBFx v3.5 Lot A — Plugin foundation tests
-- ============================================================================
-- Coverage: I-1290..I-1301 (PluginManager + Manifest + Validator + Registry)
-- Runs standalone: `bbfx-studio.exe lua/tests/test_plugin_lot_a.lua`
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot A — Plugin Foundation Tests")
print("================================================================\n")

local P, F = 0, 0
local fails = {}
local function check(id, name, cond)
    if cond then P = P + 1; print("  PASS  " .. id .. " " .. name)
    else F = F + 1; print("  FAIL  " .. id .. " " .. name); table.insert(fails, id .. " " .. name) end
end

-- ── bbfx.plugin.* namespace is exposed ──────────────────────────────────────
check("A-001", "bbfx.plugin namespace exists",
      type(bbfx.plugin) == "table")
check("A-002", "bbfx.plugin.scan is a function",
      type(bbfx.plugin.scan) == "function")
check("A-003", "bbfx.plugin.list is a function",
      type(bbfx.plugin.list) == "function")
check("A-004", "bbfx.plugin.info is a function",
      type(bbfx.plugin.info) == "function")
check("A-005", "bbfx.plugin.validatePath is a function",
      type(bbfx.plugin.validatePath) == "function")

-- ── Current BBFx version is exposed and correct ─────────────────────────────
check("A-006", "bbfx.plugin.currentBBFxVersion() == '3.5.2'",
      bbfx.plugin.currentBBFxVersion() == "3.5.2")

-- ── Scan returns a non-negative count ───────────────────────────────────────
local count = bbfx.plugin.scan()
check("A-007", "bbfx.plugin.scan() returns a number",
      type(count) == "number" and count >= 0)

-- ── list() is a table ───────────────────────────────────────────────────────
local list = bbfx.plugin.list()
check("A-008", "bbfx.plugin.list() returns a table",
      type(list) == "table")

-- ── info() on unknown plugin returns nil ────────────────────────────────────
check("A-009", "bbfx.plugin.info('bogus.does-not-exist') is nil",
      bbfx.plugin.info("bogus.does-not-exist") == nil)

-- ── userDir / bundledDir are non-empty paths ────────────────────────────────
local udir = bbfx.plugin.userDir()
local bdir = bbfx.plugin.bundledDir()
check("A-010", "bbfx.plugin.userDir() non-empty",
      type(udir) == "string" and #udir > 0)
check("A-011", "bbfx.plugin.bundledDir() non-empty",
      type(bdir) == "string" and #bdir > 0)

-- ── validatePath on a non-existent path reports ok=false with error ────────
local r = bbfx.plugin.validatePath("/this/path/does/not/exist/anywhere/12345")
check("A-012", "validatePath on missing dir returns ok=false",
      type(r) == "table" and r.ok == false and #r.errors > 0)

-- ── Write a temporary valid manifest and validate it ────────────────────────
-- We use io here because these are INTERNAL tests running outside the plugin
-- sandbox. Plugins themselves never get access to io in v3.5+.
local tmpDir = os.getenv("TEMP") or os.getenv("TMP") or "/tmp"
local testPluginDir = tmpDir .. "/bbfx_test_plugin_lot_a"
os.execute('mkdir "' .. testPluginDir .. '" 2>NUL')
os.execute('mkdir "' .. testPluginDir .. '" 2>/dev/null')

local manifestOK = [[
{
  "id": "bbfx-test.lot-a",
  "name": "Lot A Test Plugin",
  "version": "1.0.0",
  "bbfx_version": ">=3.5",
  "author": { "name": "Tests" },
  "description": "Used by test_plugin_lot_a.lua to validate PluginValidator.",
  "category": "Custom",
  "license": "MIT",
  "entry": "init.lua",
  "permissions": []
}
]]

local function writeFile(path, content)
    local f = io.open(path, "w")
    if not f then return false end
    f:write(content)
    f:close()
    return true
end

writeFile(testPluginDir .. "/manifest.json", manifestOK)
writeFile(testPluginDir .. "/init.lua", "-- test plugin entry\nreturn {}\n")

local rOk = bbfx.plugin.validatePath(testPluginDir)
check("A-013", "validatePath on valid manifest+entry is ok",
      rOk.ok == true and #rOk.errors == 0)

-- ── Malformed manifest (bad id pattern) is rejected ────────────────────────
local manifestBadId = [[
{
  "id": "NO_DOT_HERE",
  "name": "Bad",
  "version": "1.0.0",
  "bbfx_version": ">=3.5",
  "author": { "name": "Tests" },
  "description": "",
  "category": "Custom",
  "license": "MIT",
  "entry": "init.lua"
}
]]
writeFile(testPluginDir .. "/manifest.json", manifestBadId)
local rBad = bbfx.plugin.validatePath(testPluginDir)
check("A-014", "validatePath rejects malformed id",
      rBad.ok == false and #rBad.errors > 0)

-- ── Missing entry file is detected ──────────────────────────────────────────
local manifestMissingEntry = [[
{
  "id": "bbfx-test.missing-entry",
  "name": "MissingEntry",
  "version": "1.0.0",
  "bbfx_version": ">=3.5",
  "author": { "name": "Tests" },
  "description": "",
  "category": "Custom",
  "license": "MIT",
  "entry": "does_not_exist.lua"
}
]]
writeFile(testPluginDir .. "/manifest.json", manifestMissingEntry)
-- init.lua exists but entry points to missing file
local rMissing = bbfx.plugin.validatePath(testPluginDir)
check("A-015", "validatePath rejects missing entry file",
      rMissing.ok == false)

-- ── Unknown category is rejected ────────────────────────────────────────────
local manifestBadCategory = [[
{
  "id": "bbfx-test.bad-cat",
  "name": "BadCat",
  "version": "1.0.0",
  "bbfx_version": ">=3.5",
  "author": { "name": "Tests" },
  "description": "",
  "category": "Nonsense",
  "license": "MIT",
  "entry": "init.lua"
}
]]
writeFile(testPluginDir .. "/manifest.json", manifestBadCategory)
local rBadCat = bbfx.plugin.validatePath(testPluginDir)
check("A-016", "validatePath rejects unknown category",
      rBadCat.ok == false)

-- ── Incompatible bbfx_version is rejected ───────────────────────────────────
local manifestWrongVersion = [[
{
  "id": "bbfx-test.wrong-ver",
  "name": "WrongVer",
  "version": "1.0.0",
  "bbfx_version": ">=99.0",
  "author": { "name": "Tests" },
  "description": "",
  "category": "Custom",
  "license": "MIT",
  "entry": "init.lua"
}
]]
writeFile(testPluginDir .. "/manifest.json", manifestWrongVersion)
local rWrongVer = bbfx.plugin.validatePath(testPluginDir)
check("A-017", "validatePath rejects incompatible bbfx_version",
      rWrongVer.ok == false)

-- ── Cleanup test plugin ─────────────────────────────────────────────────────
os.remove(testPluginDir .. "/manifest.json")
os.remove(testPluginDir .. "/init.lua")
os.execute('rmdir "' .. testPluginDir .. '" 2>NUL')
os.execute('rmdir "' .. testPluginDir .. '" 2>/dev/null')

-- ── Summary ─────────────────────────────────────────────────────────────────
print("\n--------------------------------------------------------------")
print(string.format("  Lot A Tests: %d PASS, %d FAIL", P, F))
if F > 0 then
    print("  Failures:")
    for _, n in ipairs(fails) do print("    - " .. n) end
end
print("--------------------------------------------------------------\n")

if F > 0 then
    error("Lot A tests FAILED — aborting startup")
end

-- Exit cleanly (bbfx.exe otherwise drops into its main render loop).
os.exit(0)
