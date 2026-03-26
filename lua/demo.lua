-- demo.lua — BBFx v2.2 interactive demo
--
-- Usage:
--   bbfx.exe lua/demo.lua              → mode minimal (default)
--   bbfx.exe lua/demo.lua minimal      → ogrehead + rotation
--   bbfx.exe lua/demo.lua perlin       → ogrehead + Perlin noise deformation
--   bbfx.exe lua/demo.lua wave         → ogrehead + Wave sinusoidal deformation
--   bbfx.exe lua/demo.lua colorshift   → ogrehead + ColorShift HSV cycling
--   bbfx.exe lua/demo.lua combined     → ogrehead + Wave + ColorShift + rotation
--
-- Interactive controls:
--   1-5       Select mode (minimal/perlin/wave/colorshift/combined)
--   Up/Down   Adjust main parameter of active effect
--   F3        Toggle FPS overlay
--   F11       Toggle fullscreen
--   F12       Screenshot
--   ESC       Quit

local mode = (arg and arg[2]) or "minimal"
print("[demo] mode: " .. mode)
print("[demo] Controls:")
print("  1-5       Select mode (minimal/perlin/wave/colorshift/combined)")
print("  Up/Down   Adjust main parameter of active effect")
print("  F3        Toggle FPS overlay")
print("  F11       Toggle fullscreen")
print("  F12       Screenshot")
print("  ESC       Quit")

-- ── Keycodes ──────────────────────────────────────────────────────────────
local KEY_1 = 49  -- ASCII '1'
local KEY_2 = 50
local KEY_3 = 51
local KEY_4 = 52
local KEY_5 = 53
local KEY_UP = 1073741906
local KEY_DOWN = 1073741905

-- ── Scene setup (shared) ────────────────────────────────────────────────────

local engine = bbfx.Engine.instance()
local scene = engine:getSceneManager()

scene:setAmbientLight(Ogre.ColourValue(0.5, 0.5, 0.5))

-- Light
local light = scene:createLight("MainLight")
light:setType(Ogre.Light.LT_POINT)
light:setDiffuseColour(Ogre.ColourValue(1, 1, 1))
local lightNode = scene:getRootSceneNode():createChildSceneNode("LightNode")
lightNode:setPosition(Ogre.Vector3(100, 200, 300))
lightNode:attachObject(light)

-- Camera
local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode("CamNode")
camNode:attachObject(cam)
camNode:setPosition(Ogre.Vector3(0, 10, 200))
camNode:lookAt(Ogre.Vector3(0, 0, 0), 2) -- TS_WORLD

-- ── Mode setup functions ──────────────────────────────────────────────────

local headNode = scene:getRootSceneNode():createChildSceneNode("HeadNode")
local rotSpeed = 0.5
local currentFx = nil
local currentModeName = "minimal"

local function setupMinimal()
    local entity = scene:createEntity("Head", "ogrehead.mesh")
    headNode:attachObject(entity)
    rotSpeed = 0.5
    currentFx = nil
    currentModeName = "minimal"
    print("[demo] mode: minimal — ogrehead.mesh with rotation")
end

local function setupPerlin()
    local perlin = bbfx.PerlinVertexShader("ogrehead.mesh", "ogrehead_perlin")
    perlin:enable()
    local entity = scene:createEntity("Head", "ogrehead_perlin")
    headNode:attachObject(entity)
    rotSpeed = 0.3
    currentFx = { type = "perlin", shader = perlin, param = 0.1, paramName = "displacement", step = 0.02 }
    currentModeName = "perlin"
    print("[demo] mode: perlin — displacement=" .. currentFx.param)
end

local function setupWave()
    local wave = bbfx.WaveVertexShader("ogrehead.mesh", "ogrehead_wave")
    wave:enable()
    local entity = scene:createEntity("Head", "ogrehead_wave")
    headNode:attachObject(entity)
    rotSpeed = 0.3
    currentFx = { type = "wave", shader = wave, param = 5.0, paramName = "amplitude", step = 0.5 }
    currentModeName = "wave"
    print("[demo] mode: wave — amplitude=" .. currentFx.param)
end

local function setupColorshift()
    local entity = scene:createEntity("Head", "ogrehead.mesh")
    headNode:attachObject(entity)
    local cs = bbfx.ColorShiftNode("ogrehead")
    local anim = bbfx.Animator.instance()
    anim:addNode(cs)
    rotSpeed = 0.5
    currentFx = { type = "colorshift", node = cs, param = 60.0, paramName = "hue speed", step = 10.0 }
    currentModeName = "colorshift"
    print("[demo] mode: colorshift — hue speed=" .. currentFx.param)
end

local function setupCombined()
    local wave = bbfx.WaveVertexShader("ogrehead.mesh", "ogrehead_combined")
    wave:enable()
    local entity = scene:createEntity("Head", "ogrehead_combined")
    headNode:attachObject(entity)

    local cs = bbfx.ColorShiftNode("ogrehead")
    local anim = bbfx.Animator.instance()
    anim:addNode(cs)

    rotSpeed = 0.3
    currentFx = { type = "combined", wave = wave, colorshift = cs, param = 5.0, paramName = "amplitude", step = 0.5 }
    currentModeName = "combined"

    anim:exportDOT("graph.dot")
    print("[demo] mode: combined — Wave + ColorShift + rotation")
    print("[demo] Graph exported to graph.dot")
end

-- ── Select initial mode ───────────────────────────────────────────────────

if mode == "perlin" then
    setupPerlin()
elseif mode == "wave" then
    setupWave()
elseif mode == "colorshift" then
    setupColorshift()
elseif mode == "combined" then
    setupCombined()
else
    setupMinimal()
end

-- ── Animation (shared rotation + keyboard polling) ────────────────────────

local tn = bbfx.RootTimeNode.instance()
local animator = bbfx.Animator.instance()
local keyboard = bbfx.InputManager.instance():getKeyboard()

-- Track hue for colorshift modes
local hueAccum = 0.0

local rotNode = bbfx.LuaAnimationNode("rotate", function(self)
    local p = self:getInput("dt")
    if not p then return end
    local dt = p:getValue()

    -- Rotation
    headNode:yaw(Ogre.Radian(dt * rotSpeed))

    -- ── Keyboard: mode switching (1-5) ────────────────────────────────
    -- Note: runtime mode switching requires scene cleanup which is complex
    -- in OGRE (entity/mesh destruction). For now, print the requested mode.
    -- The mode is fully functional when passed via CLI argument.
    if keyboard:wasKeyPressed(KEY_1) then
        print("[demo] mode: minimal (restart with: bbfx.exe lua/demo.lua minimal)")
    end
    if keyboard:wasKeyPressed(KEY_2) then
        print("[demo] mode: perlin (restart with: bbfx.exe lua/demo.lua perlin)")
    end
    if keyboard:wasKeyPressed(KEY_3) then
        print("[demo] mode: wave (restart with: bbfx.exe lua/demo.lua wave)")
    end
    if keyboard:wasKeyPressed(KEY_4) then
        print("[demo] mode: colorshift (restart with: bbfx.exe lua/demo.lua colorshift)")
    end
    if keyboard:wasKeyPressed(KEY_5) then
        print("[demo] mode: combined (restart with: bbfx.exe lua/demo.lua combined)")
    end

    -- ── Keyboard: parameter adjustment (Up/Down arrows) ───────────────
    if currentFx then
        if keyboard:wasKeyPressed(KEY_UP) then
            currentFx.param = currentFx.param + currentFx.step
            print("[demo] " .. currentFx.paramName .. " = " .. currentFx.param)
        end
        if keyboard:wasKeyPressed(KEY_DOWN) then
            currentFx.param = currentFx.param - currentFx.step
            if currentFx.param < 0 then currentFx.param = 0 end
            print("[demo] " .. currentFx.paramName .. " = " .. currentFx.param)
        end
    end

    -- ── Update active effects ─────────────────────────────────────────
    if currentFx then
        if currentFx.type == "perlin" and currentFx.shader then
            -- Perlin is a FrameListener, auto-updates. Param not dynamically settable yet.
        elseif currentFx.type == "wave" and currentFx.shader then
            currentFx.shader:getInput("amplitude"):setValue(currentFx.param)
            currentFx.shader:renderOneFrame(dt)
        elseif currentFx.type == "colorshift" and currentFx.node then
            hueAccum = hueAccum + dt * currentFx.param
            currentFx.node:getInput("hue_shift"):setValue(hueAccum % 360.0)
            currentFx.node:update()
        elseif currentFx.type == "combined" then
            if currentFx.wave then
                currentFx.wave:getInput("amplitude"):setValue(currentFx.param)
                currentFx.wave:renderOneFrame(dt)
            end
            if currentFx.colorshift then
                hueAccum = hueAccum + dt * 60.0
                currentFx.colorshift:getInput("hue_shift"):setValue(hueAccum % 360.0)
                currentFx.colorshift:update()
            end
        end
    end
end)
rotNode:addInput("dt")
animator:addNode(tn)
animator:addNode(rotNode)
animator:addPort(tn, "dt", rotNode, "dt")

-- ── Info display ──────────────────────────────────────────────────────────
print("[demo] Active mode: " .. currentModeName)
if currentFx then
    print("[demo] " .. currentFx.paramName .. " = " .. currentFx.param)
end
print("[demo] Press ESC to exit")
