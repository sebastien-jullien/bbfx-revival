-- ============================================================================
-- BBFx v3.5 Lot J — GamepadManager advanced (backend coverage)
-- ============================================================================
-- Coverage: I-1385..I-1394
--   - bbfx.gamepad.* exposed + returns sane defaults without a gamepad
--   - bbfx.joystick remains as a subset alias (retrocompat)
--   - rumble / gyro / accel / calibrate are safe no-ops when no device
-- UI visualisation + learn mode arrive in Lot L (I-1403..I-1409).
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot J — GamepadManager advanced")
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

-- ── Namespace is present + legacy alias ───────────────────────────────────
check("J-001", "bbfx.gamepad namespace exists", type(bbfx.gamepad) == "table")
check("J-002", "bbfx.joystick alias exists",
      type(bbfx.joystick) == "table" and bbfx.joystick == bbfx.gamepad)

-- Key functions
for _, fn in ipairs({"count","getName","getType","getAxisValue","isButtonDown",
                    "rumble","rumbleTriggers","stopRumble",
                    "hasGyro","hasAccel","getGyro","getAccel",
                    "calibrateGyro","setGyroFilter"}) do
    check("J-003." .. fn, "bbfx.gamepad." .. fn .. " is a function",
          type(bbfx.gamepad[fn]) == "function")
end

-- ── Values that hold whether or not a physical gamepad is attached ────────
-- When bbfx.exe (headless) runs in CI with no InputManager singleton,
-- bbfx.gamepad.count() must return 0 without crashing. On a dev machine
-- with a controller, count() returns >= 1.
local count = bbfx.gamepad.count()
check("J-004", "count() returns non-negative integer",
      type(count) == "number" and count >= 0)

-- For an out-of-range index the accessors must degrade gracefully.
check("J-005", "getName(invalid) returns empty string",
      bbfx.gamepad.getName(999) == "")
check("J-006", "getType(invalid) returns 'Generic'",
      bbfx.gamepad.getType(999) == "Generic")
check("J-007", "getAxisValue(invalid) returns 0",
      bbfx.gamepad.getAxisValue(999, 0) == 0.0)
check("J-008", "isButtonDown(invalid) returns false",
      bbfx.gamepad.isButtonDown(999, 0) == false)
check("J-009", "hasGyro(invalid) returns false",
      bbfx.gamepad.hasGyro(999) == false)
check("J-010", "hasAccel(invalid) returns false",
      bbfx.gamepad.hasAccel(999) == false)

-- Gyro / accel on an invalid index return (0, 0, 0).
local gx, gy, gz = bbfx.gamepad.getGyro(999)
check("J-011", "getGyro(invalid) returns 0,0,0",
      gx == 0 and gy == 0 and gz == 0)
local ax, ay, az = bbfx.gamepad.getAccel(999)
check("J-012", "getAccel(invalid) returns 0,0,0",
      ax == 0 and ay == 0 and az == 0)

-- Rumble / calibration / setGyroFilter are safe no-ops.
check("J-013", "rumble(invalid) returns false",
      bbfx.gamepad.rumble(999, 0.5, 0.5, 100) == false)
check("J-014", "rumbleTriggers(invalid) returns false",
      bbfx.gamepad.rumbleTriggers(999, 0.5, 0.5, 100) == false)

-- Functions that return nothing (void) — we just assert pcall success.
check("J-015", "stopRumble(invalid) is a safe no-op",
      pcall(function() bbfx.gamepad.stopRumble(999) end))
check("J-016", "calibrateGyro(invalid) is a safe no-op",
      pcall(function() bbfx.gamepad.calibrateGyro(999) end))
check("J-017", "setGyroFilter applies without error",
      pcall(function() bbfx.gamepad.setGyroFilter(0.02, 0.7) end))

-- Legacy bbfx.joystick surface still works (same table).
check("J-018", "bbfx.joystick.count() matches bbfx.gamepad.count()",
      bbfx.joystick.count() == count)

print("\n--------------------------------------------------------------")
print(string.format("  Lot J Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot J tests FAILED")
end

os.exit(0)
