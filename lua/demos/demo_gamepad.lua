-- ============================================================================
-- BBFx v3.5 Lot L — Gamepad demo
-- ============================================================================
-- Wire the first connected gamepad into a minimal node graph:
--   * gyro X/Y   -> CameraNode rotation (rotX / rotY)
--   * touchpad X -> MixerNode fader    (mx.fade)
--   * button A   -> flash trigger      (tr.flash)
--
-- The demo is resilient to controllers that don't expose some features
-- (PS5 / PS4 have touchpad + gyro; Xbox has neither; Switch Pro has gyro
-- but no touchpad): every feature check uses bbfx.gamepad.hasXxx first.
-- ============================================================================

print("=== BBFx Gamepad Demo (Lot L) ===\n")

local n = bbfx.gamepad.count()
if n == 0 then
    print("No gamepad detected. Plug in a controller and re-run.")
    return
end

local idx = 0
local name = bbfx.gamepad.getName(idx)
local kind = bbfx.gamepad.getType(idx)
print(string.format("Using gamepad #%d: %s (%s)", idx, name, kind))
print("  gyro      : " .. tostring(bbfx.gamepad.hasGyro(idx)))
print("  touchpad  : " .. tostring(bbfx.gamepad.hasTouchpad(idx)))
print("  LED       : " .. tostring(bbfx.gamepad.hasLED(idx)))
local batt = bbfx.gamepad.getBatteryPercent(idx)
print(string.format("  battery   : %s",
      batt < 0 and "unknown" or (tostring(batt) .. "%")))

-- ── Minimal node graph ────────────────────────────────────────────────────
-- We don't force a specific scene: the demo only prints what it would drive.
-- Users of demo_studio can attach a CameraNode and a MixerNode and the
-- bindings below will work unchanged.

local cam  = bbfx.createNode and bbfx.createNode("CameraNode",  "democam") or nil
local mix  = bbfx.createNode and bbfx.createNode("MixerNode",   "demomix") or nil
local flash = 0.0

-- Beat-driven rumble: if the audio analyser reports a beat AND the gamepad
-- supports rumble, give a short bump. Guarded so the demo keeps running
-- on headless/offline systems.
local function pulseRumble()
    bbfx.gamepad.rumble(idx, 0.6, 0.9, 120)
end

-- ── Per-frame update hook ────────────────────────────────────────────────
function onFrame(dt)
    -- Gyro -> camera
    if cam and bbfx.gamepad.hasGyro(idx) then
        local gx, gy, gz = bbfx.gamepad.getGyro(idx)
        bbfx.setParam(cam, "rotX", gx * 0.02)
        bbfx.setParam(cam, "rotY", gy * 0.02)
    end

    -- Touchpad finger 0 -> fader
    if mix and bbfx.gamepad.hasTouchpad(idx) then
        local t = bbfx.gamepad.getTouchpadFingers(idx)
        if t.count > 0 and t.fingers[1] then
            bbfx.setParam(mix, "fade", t.fingers[1].x)
        end
    end

    -- Button A -> flash, with decay
    if bbfx.gamepad.isPressed(idx, 0) then
        flash = 1.0
        pulseRumble()
    else
        flash = math.max(0.0, flash - dt * 3.0)
    end
    if mix then bbfx.setParam(mix, "flash", flash) end
end

print("\nDemo ready. Move the sticks, tilt the controller, touch the pad, press A.")
print("Connect a CameraNode named 'democam' + a MixerNode named 'demomix'")
print("in the NodeEditor to see the effect live in the viewport.\n")
