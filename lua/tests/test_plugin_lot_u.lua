-- ============================================================================
-- BBFx v3.5 Lot U — New Plugin Wizard + Hot Reload + CLI
-- ============================================================================
-- Coverage : I-1510..I-1518 (backend surface)
--   * 6 lua/plugin/template_*.lua files are present + load-able
--   * bbfx.authoring.validatePath + validator API
--   * bbfx.hotreload.* namespace + API
--   * Wizard flow : read template -> writePlugin -> validate the result
-- Note : CLI flags (--install/--uninstall/--validate-plugin) are covered
-- by a separate shell invocation in the session log, not by this Lua
-- file (which runs inside an already-started bbfx.exe).
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot U — Wizard / HotReload / CLI")
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

-- ── Template files exist (I-1513) ──────────────────────────────────────
for _, tpl in ipairs({
    "template_node_generator",
    "template_node_fx",
    "template_preset",
    "template_shader",
    "template_panel",
    "template_output_template",
}) do
    local path = "lua/plugin/" .. tpl .. ".lua"
    check("U-001." .. tpl, tpl .. ".lua exists",
          bbfx.fs.exists(path), path)
    local body = bbfx.fs.readFile(path)
    check("U-002." .. tpl, tpl .. ".lua returns a table or has onEnable",
          type(body) == "string" and body:find("return") ~= nil)
end

-- Each template compiles as Lua syntax.
for _, tpl in ipairs({
    "template_node_generator",
    "template_node_fx",
    "template_preset",
    "template_shader",
    "template_panel",
    "template_output_template",
}) do
    local chunk, err = loadfile("lua/plugin/" .. tpl .. ".lua")
    check("U-003." .. tpl, tpl .. " parses without error",
          type(chunk) == "function", err)
end

-- ── bbfx.authoring.validatePath (I-1517) ──────────────────────────────
check("U-010", "bbfx.authoring.validatePath is a function",
      type(bbfx.authoring.validatePath) == "function")
local badRes = bbfx.authoring.validatePath("/does/not/exist")
check("U-011", "validatePath on missing dir returns {ok=false, errors=...}",
      type(badRes) == "table" and badRes.ok == false
      and type(badRes.errors) == "table" and #badRes.errors > 0)

-- Write a minimal valid plugin and validate it.
local wrote = bbfx.authoring.writePlugin(
    { id = "test-lot-u.valid", name = "Lot U Valid", version = "0.1.0",
      author = "tests", description = "Lot U" },
    "return { onEnable = function() end }\n", nil)
check("U-012", "writePlugin returned a path for Lot U fixture",
      type(wrote) == "string" and #wrote > 0)
local goodRes = bbfx.authoring.validatePath(wrote)
check("U-013", "validatePath on a valid plugin returns ok=true",
      type(goodRes) == "table" and goodRes.ok == true,
      (not goodRes.ok) and table.concat(goodRes.errors, "; ") or "")

-- ── bbfx.hotreload namespace (I-1515) ──────────────────────────────────
for _, fn in ipairs({"setEnabled","isEnabled","tick","invalidate",
                     "watchedCount","reloadsPerformed"}) do
    check("U-020." .. fn, "bbfx.hotreload." .. fn .. " is a function",
          type(bbfx.hotreload[fn]) == "function")
end

-- isEnabled default + round-trip
bbfx.hotreload.setEnabled(true)
check("U-021", "hotreload.isEnabled() = true after setEnabled(true)",
      bbfx.hotreload.isEnabled() == true)
bbfx.hotreload.setEnabled(false)
check("U-022", "hotreload.isEnabled() = false after setEnabled(false)",
      bbfx.hotreload.isEnabled() == false)
bbfx.hotreload.setEnabled(true)

-- watchedCount / reloadsPerformed return ints
check("U-023", "hotreload.watchedCount() returns int",
      type(bbfx.hotreload.watchedCount()) == "number")
check("U-024", "hotreload.reloadsPerformed() returns int",
      type(bbfx.hotreload.reloadsPerformed()) == "number")

-- tick() must not throw
local ok = pcall(function() bbfx.hotreload.tick() end)
check("U-025", "hotreload.tick() does not throw", ok)
bbfx.hotreload.invalidate()
check("U-026", "hotreload.invalidate() is callable", true)

-- ── Wizard flow (I-1510..1514) ─────────────────────────────────────────
-- Read a template body and export a new plugin using it.
local templateBody = bbfx.fs.readFile("lua/plugin/template_node_fx.lua")
check("U-030", "template_node_fx.lua is readable",
      type(templateBody) == "string" and #templateBody > 0)
local wizardPath = bbfx.authoring.writePlugin(
    { id = "test-lot-u.wizard", name = "Wizard Test", category = "Custom",
      author = "tests", description = "Generated via template wizard" },
    templateBody, nil)
check("U-031", "wizard writePlugin returned a path",
      type(wizardPath) == "string" and #wizardPath > 0)
local wizardInit = bbfx.fs.readFile(wizardPath .. "/init.lua")
check("U-032", "wizard init.lua contains the template body",
      type(wizardInit) == "string" and wizardInit:find("registerNodeType") ~= nil)
local wizardRes = bbfx.authoring.validatePath(wizardPath)
check("U-033", "wizard plugin validates OK",
      wizardRes.ok == true,
      (not wizardRes.ok) and table.concat(wizardRes.errors, "; ") or "")

-- Cleanup
local function rmdir(p)
    os.execute('rmdir /S /Q "' .. p:gsub("/", "\\") .. '" 2>NUL')
    os.execute('rm -rf "' .. p .. '" 2>/dev/null')
end
rmdir(wrote)
rmdir(wizardPath)

-- ── Non-regression : Lot T/S bindings still reachable ──────────────────
check("U-099.a", "bbfx.renderTexture.create still reachable",
      type(bbfx.renderTexture.create) == "function")
check("U-099.b", "bbfx.authoring.slugify still reachable",
      type(bbfx.authoring.slugify) == "function")
check("U-099.c", "bbfx.noise.simplex2D still reachable",
      type(bbfx.noise.simplex2D) == "function")

print("\n--------------------------------------------------------------")
print(string.format("  Lot U Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot U tests FAILED")
end

os.exit(0)
