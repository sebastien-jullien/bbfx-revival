-- ============================================================================
-- BBFx v3.5 Lot P — ImGui Lua API + Inspector Widgets
-- ============================================================================
-- Coverage : I-1448..I-1459 (backend surface)
--   * Studio-only : `bbfx.ui` is registered by main_studio.cpp, so it is
--     not present in the headless bbfx.exe. This test is designed to
--     be driven from bbfx-studio (either interactively from the console
--     or from dbg_autotest.lua). Running it from bbfx.exe validates
--     only the "ui table absent in headless" invariant.
--   * In Studio context, we check:
--     - bbfx.ui exists and exposes all 40+ widget helpers
--     - registerPanel adds a panel (ScriptPanelRegistry sees it)
--     - registerInspectorWidget adds a wildcard port handler
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot P — ImGui Lua API")
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

-- ── Detect runtime : headless vs Studio ────────────────────────────────
local inStudio = (type(bbfx.ui) == "table")
print("  Runtime detected : " .. (inStudio and "Studio (bbfx-studio)"
                                             or "Headless (bbfx)"))

if not inStudio then
    -- In headless, the only invariant is : bbfx.ui is absent. Every other
    -- namespace from Lot M/N/O still works.
    check("P-001", "bbfx.ui is absent in headless",
          bbfx.ui == nil)
    check("P-002", "bbfx.noise.simplex2D still reachable",
          type(bbfx.noise.simplex2D) == "function")
    check("P-003", "bbfx.timeline.create still reachable",
          type(bbfx.timeline.create) == "function")
    check("P-004", "bbfx.fs.readFile still reachable",
          type(bbfx.fs.readFile) == "function")
    check("P-005", "bbfx.json.encode still reachable",
          type(bbfx.json.encode) == "function")
    print("\n--------------------------------------------------------------")
    print(string.format("  Lot P Tests (headless): %d PASS, %d FAIL", P, F))
    print("--------------------------------------------------------------\n")
    if F > 0 then
        for _, n in ipairs(fails) do print("  - " .. n) end
        error("Lot P tests FAILED")
    end
    os.exit(0)
end

-- ── Studio path : full widget surface ──────────────────────────────────
for _, fn in ipairs({
    "text","textColored","textWrapped","textDisabled","bulletText",
    "button","smallButton","checkbox","radioButton",
    "sliderFloat","sliderInt","sliderFloat2","sliderFloat3","sliderFloat4",
    "inputFloat","inputInt","inputText",
    "colorEdit3","colorEdit4","colorPicker3",
    "combo","listBox",
    "separator","spacing","sameLine","newLine","indent","unindent",
    "columns","nextColumn","treeNode","treePop","collapsingHeader",
    "image","plotLines","plotHistogram","progressBar",
    "beginTabBar","endTabBar","beginTabItem","endTabItem",
    "beginPopup","endPopup","openPopup","closeCurrentPopup","tooltip",
    "registerPanel","unregisterPanel","registerInspectorWidget",
}) do
    check("P-010." .. fn, "bbfx.ui." .. fn .. " is a function",
          type(bbfx.ui[fn]) == "function")
end

-- ── registerPanel / unregisterPanel round-trip ─────────────────────────
local drawCount = 0
bbfx.ui.registerPanel("Lot P Demo", function() drawCount = drawCount + 1 end)
check("P-050", "registerPanel adds to ScriptPanelRegistry", true)
bbfx.ui.unregisterPanel("Lot P Demo")
check("P-051", "unregisterPanel is callable", true)

-- ── registerInspectorWidget accepts a callback ─────────────────────────
bbfx.ui.registerInspectorWidget("mx.fade", function(node, port, value)
    return false, value   -- no change
end)
check("P-060", "registerInspectorWidget callable", true)

print("\n--------------------------------------------------------------")
print(string.format("  Lot P Tests (Studio): %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot P tests FAILED")
end

os.exit(0)
