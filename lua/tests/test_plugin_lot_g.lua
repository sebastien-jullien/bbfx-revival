-- ============================================================================
-- BBFx v3.5 Lot G — Plugin Manager + Errors Panel (backend coverage)
-- ============================================================================
-- Coverage: I-1345..I-1356 (backend wiring of the UI)
--   - PluginErrorLog is populated by sandbox violations / load errors
--   - bbfx.plugin.info surface is sufficient to build a manager UI
--   - bbfx.plugin.install/uninstall pipeline is the source of truth the
--     panel lists from
-- UI click-driven tests (imgui_test_engine) are deferred to Lot W.
-- Runs standalone: `bbfx.exe lua/tests/test_plugin_lot_g.lua`
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot G — Plugin Manager Panel backend coverage")
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

-- ── Setup: install a good + a bad plugin and trigger a sandbox violation
local tmp = (os.getenv("TEMP") or os.getenv("TMP") or "/tmp"):gsub("[/\\]+$", "")
local srcGood = tmp .. "/bbfx_lotg_good"
local srcBad  = tmp .. "/bbfx_lotg_bad"

local function wipe(p)
    local winP = p:gsub("/", "\\")
    os.execute('rmdir /S /Q "' .. winP .. '" 2>NUL')
    os.execute('rm -rf "' .. p .. '" 2>/dev/null')
end
local function mkdir(p)
    local winP = p:gsub("/", "\\")
    os.execute('mkdir "' .. winP .. '" 2>NUL')
    os.execute('mkdir -p "' .. p .. '" 2>/dev/null')
end
local function writeFile(p, c)
    local f = io.open(p, "w"); f:write(c); f:close()
end

wipe(srcGood); wipe(srcBad)
mkdir(srcGood); mkdir(srcBad)

writeFile(srcGood .. "/manifest.json", [[
{ "id": "bbfx-lotg.good", "name": "Lot G Good", "version": "1.0.0",
  "bbfx_version": ">=3.5", "author": { "name": "Tests" },
  "description": "Well-behaved.", "category": "Custom", "license": "MIT",
  "entry": "init.lua" }
]])
writeFile(srcGood .. "/init.lua", "return {}\n")

writeFile(srcBad .. "/manifest.json", [[
{ "id": "bbfx-lotg.bad", "name": "Lot G Bad", "version": "1.0.0",
  "bbfx_version": ">=3.5", "author": { "name": "Tests" },
  "description": "Triggers a sandbox violation on load.",
  "category": "Custom", "license": "MIT", "entry": "init.lua" }
]])
-- require("ffi") fires a sandbox violation — the manager must show it FAILED
writeFile(srcBad .. "/init.lua", [[
local r = require("ffi")
return {}
]])

-- Clean any previous run
if bbfx.plugin.info("bbfx-lotg.good") then bbfx.plugin.uninstall("bbfx-lotg.good") end
if bbfx.plugin.info("bbfx-lotg.bad")  then bbfx.plugin.uninstall("bbfx-lotg.bad")  end

check("G-001", "install good plugin",
      bbfx.plugin.install(srcGood).ok == true)
check("G-002", "install bad plugin",
      bbfx.plugin.install(srcBad).ok == true)

-- Enable the good one.
check("G-003", "enable good plugin",
      bbfx.plugin.enable("bbfx-lotg.good") == true)

-- Attempt to enable the bad one — expected to FAIL at load.
bbfx.plugin.enable("bbfx-lotg.bad")
local infoBad = bbfx.plugin.info("bbfx-lotg.bad")
check("G-004", "bad plugin ends up FAILED",
      infoBad ~= nil and infoBad.state == "FAILED",
      infoBad and infoBad.state)

-- The PluginErrorLog (backing the Errors panel) received the entry.
-- We don't expose a full Lua binding for the log here (arrives in Lot W
-- polish), but we can at least assert that `last_error` is non-empty on
-- the FAILED plugin, which is what the panel renders from.
check("G-005", "bad plugin has a last_error populated",
      infoBad ~= nil and type(infoBad.last_error) == "string"
                    and #infoBad.last_error > 0,
      infoBad and infoBad.last_error)

-- The good plugin remains enabled and healthy.
local infoGood = bbfx.plugin.info("bbfx-lotg.good")
check("G-006", "good plugin is still ENABLED",
      infoGood ~= nil and infoGood.state == "ENABLED")

-- Uninstall clean-up propagates: the plugin disappears from list().
bbfx.plugin.uninstall("bbfx-lotg.good")
bbfx.plugin.uninstall("bbfx-lotg.bad")
check("G-007", "uninstall removes entries from list()",
      bbfx.plugin.info("bbfx-lotg.good") == nil and
      bbfx.plugin.info("bbfx-lotg.bad")  == nil)

wipe(srcGood); wipe(srcBad)

print("\n--------------------------------------------------------------")
print(string.format("  Lot G Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot G tests FAILED")
end

os.exit(0)
