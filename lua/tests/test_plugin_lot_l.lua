-- ============================================================================
-- BBFx v3.5 Lot L — GamepadPanel bindings + Learn + Demo
-- ============================================================================
-- Coverage: I-1403..I-1409 (backend surface)
--   - Convenience accessors bbfx.gamepad.getLeftStick/getRightStick/
--     getTriggers/isPressed exist and return the expected shapes even on
--     invalid indices (no crash — defensive design).
--   - calibrateGyro on invalid index is a no-op (does not throw).
--   - setGyroFilter accepts any (processNoise, measurementNoise) pair.
--   - demo_gamepad.lua loads without error (syntax + bindings surface ok).
-- UI-level assertions (panel draws, sphere rotates, learn mode) are
-- covered by the imgui_test_engine tests in the Studio suite.
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot L — GamepadPanel + Learn + Demo")
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

-- ── Convenience accessors exist (I-1409) ─────────────────────────────────
for _, fn in ipairs({"getLeftStick","getRightStick","getTriggers","isPressed"}) do
    check("L-001." .. fn, "bbfx.gamepad." .. fn .. " is a function",
          type(bbfx.gamepad[fn]) == "function")
end

-- ── Shape on invalid index (no gamepad) ───────────────────────────────────
local lx, ly = bbfx.gamepad.getLeftStick(999)
check("L-002", "getLeftStick(invalid) returns (0, 0)",
      lx == 0 and ly == 0)

local rx, ry = bbfx.gamepad.getRightStick(999)
check("L-003", "getRightStick(invalid) returns (0, 0)",
      rx == 0 and ry == 0)

local lt, rt = bbfx.gamepad.getTriggers(999)
check("L-004", "getTriggers(invalid) returns (0, 0)",
      lt == 0 and rt == 0)

check("L-005", "isPressed(invalid, 0) = false",
      bbfx.gamepad.isPressed(999, 0) == false)

-- ── Defensive: calibrateGyro / setGyroFilter tolerate invalid state ────
local okCalib = pcall(function() bbfx.gamepad.calibrateGyro(999) end)
check("L-006", "calibrateGyro(invalid) does not throw", okCalib)

local okFilt = pcall(function() bbfx.gamepad.setGyroFilter(0.02, 0.5) end)
check("L-007", "setGyroFilter accepts (0.02, 0.5)", okFilt)

-- ── Triggers >= 0 even if axis negative on some controllers ─────────────
-- (Already true by construction — the binding clamps with std::max(0, v).)
local lt2, rt2 = bbfx.gamepad.getTriggers(0)
check("L-008", "getTriggers(0) clamps to >= 0",
      lt2 >= 0 and rt2 >= 0)

-- ── Coverage of the complete Lot J/K/L surface ──────────────────────────
for _, fn in ipairs({
    "count","getName","getType",
    "getAxisValue","isButtonDown","isPressed",
    "getLeftStick","getRightStick","getTriggers",
    "rumble","rumbleTriggers","stopRumble",
    "hasGyro","hasAccel","getGyro","getAccel",
    "calibrateGyro","setGyroFilter",
    "hasTouchpad","getTouchpadFingers",
    "hasLED","setLED",
    "getBatteryPercent","getBatteryState",
}) do
    check("L-009." .. fn, "bbfx.gamepad." .. fn .. " exists",
          type(bbfx.gamepad[fn]) == "function")
end

-- ── demo_gamepad.lua loads without error (I-1409) ───────────────────────
-- loadfile only parses — we don't run it because it defines onFrame and
-- relies on a live engine loop.
local chunk, err = loadfile("lua/demos/demo_gamepad.lua")
check("L-010", "lua/demos/demo_gamepad.lua parses without error",
      type(chunk) == "function", err)

-- ── Non-regression : Lot K's touchpad + LED + battery still work ────────
check("L-011", "hasTouchpad(invalid) = false",
      bbfx.gamepad.hasTouchpad(999) == false)
check("L-012", "hasLED(invalid) = false",
      bbfx.gamepad.hasLED(999) == false)
check("L-013", "getBatteryPercent(invalid) = -1",
      bbfx.gamepad.getBatteryPercent(999) == -1)
check("L-014", "getBatteryState(invalid) = Unknown",
      bbfx.gamepad.getBatteryState(999) == "Unknown")

-- ── Mapping round-trip (Lot K) still works — direct non-regression ─────
local tmp = (os.getenv("TEMP") or os.getenv("TMP") or "/tmp"):gsub("[/\\]+$", "")
local savePath = tmp .. "/bbfx_lot_l_mapping.bbfx-gamepad-mapping"
local saved = bbfx.gamepadMapping.saveFile(savePath, {
    name = "Lot L Mapping", deviceType = "PS5", description = "Lot L test",
    mappings = {
        { source = "leftStickX", target = "cam.rotY", scale = 1, offset = 0, invert = false },
    },
})
check("L-015", "gamepadMapping.saveFile round-trip works after Lot L",
      saved == true)
local loaded = bbfx.gamepadMapping.loadFile(savePath)
check("L-016", "gamepadMapping.loadFile round-trip works after Lot L",
      type(loaded) == "table" and loaded.ok == true and #loaded.mappings == 1)
os.remove(savePath)

print("\n--------------------------------------------------------------")
print(string.format("  Lot L Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot L tests FAILED")
end

os.exit(0)
