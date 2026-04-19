-- ============================================================================
-- BBFx v3.5 Lot K — Touchpad + LED + Battery + GamepadNode + Profiles
-- ============================================================================
-- Coverage: I-1395..I-1402 (backend coverage)
--   - bbfx.gamepad.hasTouchpad / getTouchpadFingers / hasLED / setLED /
--     getBatteryPercent / getBatteryState on invalid indices
--   - bbfx.gamepadMapping.loadFile + saveFile round-trip
--   - The 3 shipped profiles (PS5 VJ, Xbox DJ, SwitchPro Performance)
--     load without error
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot K — Touchpad + LED + Battery + Mappings")
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

-- ── Lot K additions on bbfx.gamepad ───────────────────────────────────────
for _, fn in ipairs({"hasTouchpad","getTouchpadFingers","hasLED","setLED",
                    "getBatteryPercent","getBatteryState"}) do
    check("K-001." .. fn, "bbfx.gamepad." .. fn .. " is a function",
          type(bbfx.gamepad[fn]) == "function")
end

check("K-002", "hasTouchpad(invalid) = false",
      bbfx.gamepad.hasTouchpad(999) == false)

local tp = bbfx.gamepad.getTouchpadFingers(999)
check("K-003", "getTouchpadFingers(invalid) returns table with count=0",
      type(tp) == "table" and tp.count == 0)

check("K-004", "hasLED(invalid) = false",
      bbfx.gamepad.hasLED(999) == false)

check("K-005", "setLED(invalid) returns false",
      bbfx.gamepad.setLED(999, 255, 0, 128) == false)

check("K-006", "getBatteryPercent(invalid) = -1",
      bbfx.gamepad.getBatteryPercent(999) == -1)

check("K-007", "getBatteryState(invalid) = 'Unknown'",
      bbfx.gamepad.getBatteryState(999) == "Unknown")

-- ── GamepadMappingProfile load/save round-trip ────────────────────────────
check("K-008", "bbfx.gamepadMapping namespace exists",
      type(bbfx.gamepadMapping) == "table")

local tmp = (os.getenv("TEMP") or os.getenv("TMP") or "/tmp"):gsub("[/\\]+$", "")
local savePath = tmp .. "/bbfx_lot_k_mapping.bbfx-gamepad-mapping"

local profile = {
    name = "Test Mapping",
    deviceType = "PS5",
    description = "Unit test mapping — safe to delete.",
    mappings = {
        { source = "gyroX",       target = "cam.rotX", scale = 0.02, offset = 0.0, invert = false },
        { source = "touchpad0X",  target = "mx.fade",  scale = 1.0,  offset = 0.0, invert = true  },
        { source = "buttonA",     target = "tr.flash", scale = 1.0,  offset = 0.0, invert = false },
    },
}
check("K-009", "saveFile writes the profile",
      bbfx.gamepadMapping.saveFile(savePath, profile) == true)

local loaded = bbfx.gamepadMapping.loadFile(savePath)
check("K-010", "loadFile returns ok=true on a profile we just wrote",
      type(loaded) == "table" and loaded.ok == true
      and loaded.name == "Test Mapping"
      and loaded.deviceType == "PS5"
      and #loaded.mappings == 3)

check("K-011", "first mapping round-trips correctly",
      loaded.mappings[1].source == "gyroX"
      and loaded.mappings[1].target == "cam.rotX"
      and math.abs(loaded.mappings[1].scale - 0.02) < 1e-4)

check("K-012", "invert flag preserved on the second mapping",
      loaded.mappings[2].invert == true)

-- Invalid path
local bad = bbfx.gamepadMapping.loadFile("/does/not/exist/nope.bbfx-gamepad-mapping")
check("K-013", "loadFile returns {ok=false, error=...} on missing path",
      type(bad) == "table" and bad.ok == false and type(bad.error) == "string")

os.remove(savePath)

-- ── Shipped profiles load without error ──────────────────────────────────
-- resources/mappings/*.bbfx-gamepad-mapping is copied into the runtime
-- working dir by the build step; look relative to the exe.
local shipped = {
    { path = "resources/mappings/ps5_vj_default.bbfx-gamepad-mapping",   device = "PS5"      },
    { path = "resources/mappings/xbox_dj_setup.bbfx-gamepad-mapping",    device = "Xbox"     },
    { path = "resources/mappings/switchpro_performance.bbfx-gamepad-mapping", device = "SwitchPro" },
}
for i, s in ipairs(shipped) do
    local r = bbfx.gamepadMapping.loadFile(s.path)
    check(string.format("K-014.%d", i),
          "shipped " .. s.device .. " mapping loads",
          type(r) == "table" and r.ok == true and #r.mappings > 0,
          r and (r.error or r.deviceType))
end

print("\n--------------------------------------------------------------")
print(string.format("  Lot K Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot K tests FAILED")
end

os.exit(0)
