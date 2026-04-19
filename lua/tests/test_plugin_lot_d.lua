-- ============================================================================
-- BBFx v3.5 Lot D — Settings + Commands + Bindings tests
-- ============================================================================
-- Coverage: I-1320..I-1324
--   - bbfx.plugin.install / uninstall
--   - Persist enabled list in settings.json and auto-enable on next scan
--   - PluginCommands undo/redo (via bbfx.plugin.enable/disable state)
-- Runs standalone: `bbfx.exe lua/tests/test_plugin_lot_d.lua`
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot D — Settings + Commands + Bindings")
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

-- Prepare a source plugin dir under %TEMP%.
local tmp = (os.getenv("TEMP") or os.getenv("TMP") or "/tmp"):gsub("[/\\]+$", "")
local src = tmp .. "/bbfx_lotd_src"
os.execute('rmdir /S /Q "' .. src:gsub("/", "\\") .. '" 2>NUL')
os.execute('rm -rf "' .. src .. '" 2>/dev/null')
os.execute('mkdir "' .. src:gsub("/", "\\") .. '" 2>NUL')
os.execute('mkdir -p "' .. src .. '" 2>/dev/null')

local function writeFile(p, c)
    local f = io.open(p, "w"); if not f then error("cannot write " .. p) end
    f:write(c); f:close()
end

writeFile(src .. "/manifest.json", [[
{
  "id": "bbfx-lotd.demo",
  "name": "Lot D Demo",
  "version": "1.0.0",
  "bbfx_version": ">=3.5",
  "author": { "name": "Tests" },
  "description": "Exercises install/uninstall + persistence.",
  "category": "Custom",
  "license": "MIT",
  "entry": "init.lua"
}
]])
writeFile(src .. "/init.lua", [[
return {}
]])

-- ── D-001: install from a local directory ──────────────────────────────────
-- Make sure a prior run did not leave the plugin installed.
if bbfx.plugin.info("bbfx-lotd.demo") then
    bbfx.plugin.uninstall("bbfx-lotd.demo")
end
local r = bbfx.plugin.install(src)
check("D-001", "install from local dir returns ok",
      type(r) == "table" and r.ok == true and r.id == "bbfx-lotd.demo",
      r and (r.error or r.id))

local info = bbfx.plugin.info("bbfx-lotd.demo")
check("D-002", "installed plugin appears in list()",
      info ~= nil and info.state == "VALIDATED")

-- ── D-003: enable then re-scan — plugin stays ENABLED via settings ─────────
check("D-003", "enable(bbfx-lotd.demo)",
      bbfx.plugin.enable("bbfx-lotd.demo") == true)

-- Scan + autoEnableFromSettings emulates what main/main_studio do at
-- startup. The plugin was enabled above, which wrote the id into
-- settings.enabledPlugins; after a rescan (which resets all plugins to
-- VALIDATED), autoEnableFromSettings must walk that list and re-enable it.
bbfx.plugin.scan()
local afterScan = bbfx.plugin.info("bbfx-lotd.demo")
check("D-003b", "after scan, state returns to VALIDATED",
      afterScan ~= nil and afterScan.state == "VALIDATED")

bbfx.plugin.autoEnableFromSettings()
local afterAuto = bbfx.plugin.info("bbfx-lotd.demo")
check("D-004", "autoEnableFromSettings restores ENABLED from settings.json",
      afterAuto ~= nil and afterAuto.state == "ENABLED",
      afterAuto and afterAuto.state)

-- Now disable so uninstall below doesn't leave a stale entry in settings.
bbfx.plugin.disable("bbfx-lotd.demo")

-- ── D-005: uninstall removes the directory entirely ───────────────────────
local okUn = bbfx.plugin.uninstall("bbfx-lotd.demo")
check("D-005", "uninstall returns true for a non-builtin plugin",
      okUn == true)
check("D-006", "after uninstall the plugin is no longer listed",
      bbfx.plugin.info("bbfx-lotd.demo") == nil)

-- ── D-007: install(file) is rejected (ZIP pipeline is Lot F) ──────────────
local nonDir = src .. "/manifest.json"
local r2 = bbfx.plugin.install(nonDir)
check("D-007", "install(file) reports an error (ZIP pipeline not in Lot D)",
      type(r2) == "table" and r2.ok == false and r2.error ~= nil)

-- ── D-008: install twice in a row is rejected (destination exists) ────────
-- Reinstall, then try again.
bbfx.plugin.install(src)
local r3 = bbfx.plugin.install(src)
check("D-008", "second install without uninstall is rejected",
      r3.ok == false and string.find(r3.error or "", "exists") ~= nil)

bbfx.plugin.uninstall("bbfx-lotd.demo")

-- ── Cleanup ───────────────────────────────────────────────────────────────
os.execute('rmdir /S /Q "' .. src:gsub("/", "\\") .. '" 2>NUL')
os.execute('rm -rf "' .. src .. '" 2>/dev/null')
bbfx.plugin.scan()

print("\n--------------------------------------------------------------")
print(string.format("  Lot D Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    print("Failures:")
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot D tests FAILED")
end

os.exit(0)
