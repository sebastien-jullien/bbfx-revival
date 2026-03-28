-- demo_studio.lua
-- BBFx Studio v3.0 — Default showcase scene
-- Demonstrates: OGRE viewport, DAG with linked nodes, real-time control via Studio UI

print("[demo_studio] BBFx Studio v3.0")

local engine = bbfx.Engine.instance()
local scene = engine:getSceneManager()
local animator = bbfx.Animator.instance()

-- ── Scene setup ──────────────────────────────────────────────────────────────

-- Ambient + directional light for better illumination
scene:setAmbientLight(ColourValue(0.3, 0.3, 0.35))

local light = scene:createLight("StudioLight")
light:setType(Light.LT_POINT)
light:setDiffuseColour(ColourValue(0.9, 0.85, 0.8))
light:setSpecularColour(ColourValue(1, 1, 1))
local lightNode = scene:getRootSceneNode():createChildSceneNode("StudioLightNode")
lightNode:setPosition(Vector3(50, 80, 120))
lightNode:attachObject(light)

-- Load ogrehead
local headNode = scene:getRootSceneNode():createChildSceneNode("StudioHeadNode")
local ok, err = pcall(function()
    local entity = scene:createEntity("StudioHead", "ogrehead.mesh")
    headNode:attachObject(entity)
    print("[demo_studio] ogrehead.mesh loaded")
end)
if not ok then
    print("[demo_studio] Mesh not available: " .. tostring(err))
end

-- Camera: attach to a SceneNode and position it to see the ogrehead
local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode("CameraNode")
camNode:setPosition(Vector3(0, 25, 150))
camNode:lookAt(Vector3(0, 0, 0), 1)  -- 1 = TS_WORLD
camNode:attachObject(cam)

-- ── DAG: animation graph ─────────────────────────────────────────────────────
-- RootTimeNode "time" is already created and registered by main_studio.cpp
-- It outputs: dt (frame delta) and total (elapsed seconds)

-- 1. Rotation node: continuously rotates head around Y axis.
--    time  = elapsed seconds (from RootTimeNode.total)
--    speed = rotation speed multiplier (default 1.0, editable in Inspector)
--    angle = current yaw angle output (radians)
local rotNode = bbfx.LuaAnimationNode("rotate_head", function(node)
    local t = node:getInput("time")  and node:getInput("time"):getValue()  or 0
    local s = node:getInput("speed") and node:getInput("speed"):getValue() or 1.0
    local angle = t * s
    headNode:setOrientation(Quaternion(Radian(angle), Vector3(0, 1, 0)))
    local out = node:getOutput("angle")
    if out then out:setValue(angle) end
end)
rotNode:addInput("time")
rotNode:addInput("speed")
rotNode:addOutput("angle")
animator:addNode(rotNode)

-- 2. Oscillator node: produces a sine wave mapped to [min, max].
--    time      = elapsed seconds (clock input)
--    frequency = oscillation frequency in Hz (default 0.5)
--    min       = output minimum (default 0, editable in Inspector)
--    max       = output maximum (default 1, editable in Inspector)
--    value     = output signal
local oscNode = bbfx.LuaAnimationNode("oscillator", function(node)
    local t   = node:getInput("time")      and node:getInput("time"):getValue()      or 0
    local f   = node:getInput("frequency") and node:getInput("frequency"):getValue() or 0.5
    local lo  = node:getInput("min")       and node:getInput("min"):getValue()        or 0
    local hi  = node:getInput("max")       and node:getInput("max"):getValue()        or 1
    local norm = math.sin(t * f * 2 * math.pi) * 0.5 + 0.5   -- 0..1
    local value = lo + (hi - lo) * norm
    local outPort = node:getOutput("value")
    if outPort then outPort:setValue(value) end
end)
oscNode:addInput("time")
oscNode:addInput("frequency")
oscNode:addInput("min")
oscNode:addInput("max")
oscNode:addOutput("value")
animator:addNode(oscNode)

-- 3. Link timer.total → oscillator.time (the oscillator needs the clock)
animator:addPort(
    animator:getNodeByName("time"), "total",
    animator:getNodeByName("oscillator"), "time"
)

-- Set default oscillator values
oscNode:getInput("frequency"):setValue(0.5)
oscNode:getInput("min"):setValue(0)
oscNode:getInput("max"):setValue(1)

-- 4. Link timer.total → rotate_head.time (continuous rotation driven by elapsed time)
animator:addPort(
    animator:getNodeByName("time"), "total",
    animator:getNodeByName("rotate_head"), "time"
)

-- Set default rotation speed (1.0 rad/s)
rotNode:getInput("speed"):setValue(1.0)

print("[demo_studio] Studio scene ready.")
print("")
print("  DAG graph:")
print("    time.total ──▶ oscillator.time")
print("    time.total ──▶ rotate_head.time")
print("")
print("  Try:")
print("    - Select 'rotate_head' → drag 'speed' slider in Inspector")
print("    - Select 'oscillator' → drag 'frequency' / 'min' / 'max' sliders")
print("    - Drag oscillator.value → rotate_head.speed in Node Editor")
print("    - F5: Performance Mode | ESC: Quit")
