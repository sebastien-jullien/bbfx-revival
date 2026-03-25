-- test_longrun.lua
-- Stability test: runs the engine for 60 seconds then exits cleanly.
-- Usage: bbfx.exe tests/test_longrun.lua
--
-- The script sets up a minimal scene with a rotating node, lets the engine
-- run for 60 seconds, then calls Engine:stopRendering(). Validation: exit code 0.

local DURATION = 60.0  -- seconds

print(string.format("[longrun] Starting %d-second stability test...", DURATION))

-- ── Scene setup ────────────────────────────────────────────────────────────
local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()

scene:setAmbientLight(Ogre.ColourValue(0.5, 0.5, 0.5))

local light = scene:createLight("LongRunLight")
light:setType(Ogre.Light.LT_POINT)
light:setDiffuseColour(Ogre.ColourValue(1, 1, 1))

local root_node = scene:getRootSceneNode()
local spin_node = root_node:createChildSceneNode("SpinNode")
spin_node:setPosition(Ogre.Vector3(0, 0, 0))

-- ── Timer node ─────────────────────────────────────────────────────────────
local animator = bbfx.Animator.instance()
local time_node = bbfx.RootTimeNode("longrun_timer")
animator:addNode(time_node)

-- ── Watchdog node — stops engine after DURATION seconds ────────────────────
local elapsed = 0.0
local watchdog = bbfx.LuaAnimationNode("watchdog", function(self)
    local dt = time_node:getOutput("dt"):getValue()
    if type(dt) == "number" then
        elapsed = elapsed + dt
    end

    -- Rotate the spin node each frame to exercise the scene graph
    spin_node:yaw(Ogre.Radian(0.01))

    if elapsed >= DURATION then
        print(string.format("[longrun] %d seconds elapsed — stopping engine.", DURATION))
        engine:stopRendering()
    end
end)
animator:addNode(watchdog)

-- ── Link timer → watchdog ──────────────────────────────────────────────────
animator:addPort(time_node, "dt", watchdog, "dt")

print("[longrun] Scene ready. Entering render loop...")
