-- demo_set.lua — BBFx v2.3 Playable VJ Demo Set
-- The first playable BBFx set since 2006.

require 'helpers'
require 'object'
require 'effect'
require 'camera'
require 'compositors'
require 'joystick_mapping'
require 'keymap'
require 'note'
require 'chord'
require 'sync'
require 'threads'
require 'ogre_controller'

bbfx_globals()

print("=== BBFx v2.3 — Demo Set ===")
print("")
print("Controls:")
print("  Joystick 1 axis 1: Perlin displacement")
print("  Joystick 2 axes: Camera orbit")
print("  F4-F7: Chord states (scene presets)")
print("  B: Toggle Bloom compositor")
print("  P: Toggle particles")
print("  Up/Down: Adjust Perlin displacement")
print("  ESC: Quit")
print("")

-- ── Scene setup ──────────────────────────────────────────────────────────

Effect.ambient(0.3, 0.3, 0.3)
Effect.bg(Ogre.ColourValue(0.05, 0.05, 0.15))

-- Light
local mainLight = Object:fromLight(Ogre.Light.LT_POINT, Ogre.ColourValue(1, 1, 1))
mainLight:setPosition(Ogre.Vector3(200, 300, 200))

-- ── Objects ──────────────────────────────────────────────────────────────

-- Main mesh with Perlin
local perlin = bbfx.PerlinVertexShader("ogrehead.mesh", "set_perlin")
perlin:enable()
local geoObj = Object:fromMesh("set_perlin")

-- Wave mesh (hidden initially)
local wave = bbfx.WaveVertexShader("ogrehead.mesh", "set_wave")
wave:enable()
local waveObj = Object:fromMesh("set_wave")
waveObj:setPosition(Ogre.Vector3(100, 0, 0))
waveObj:detach()

-- ColorShift on main mesh
local colorShift = bbfx.ColorShiftNode("ogrehead")

-- ── Particle systems ─────────────────────────────────────────────────────

local aureola = nil
local aureolaVisible = false
local ok, psys = pcall(function()
    return Ogre.createParticleSystem(scene, "setAureola", "Examples/Aureola")
end)
if ok and psys then
    local psysNode = scene:getRootSceneNode():createChildSceneNode("aureolaNode")
    psysNode:attachObject(psys)
    aureola = psys
    print("[set] Aureola particles loaded")
end

-- ── Camera ───────────────────────────────────────────────────────────────

local cam = scene:getCamera("MainCamera")
local camNode = cam:getParentSceneNode() or scene:getRootSceneNode():createChildSceneNode("setCamNode")
local azimuth = 0.0
local elevation = 0.3
local distance = 200.0

-- ── Composition engine ───────────────────────────────────────────────────

-- Notes
local notePerlin = Note:fromObject(geoObj)
local noteWave = Note:fromObject(waveObj)
local noteSkybox = Note:fromEffect(Effect.skybox, true, "Examples/SceneSkyBox1")

-- Chord: 3 scene presets
local mainChord = Chord:create()
mainChord:addState(1) -- Perlin only
mainChord:defState(1, notePerlin, 0, 999, 1.0)

mainChord:addState(2) -- Perlin + Wave
mainChord:defState(2, notePerlin, 0, 999, 1.0)
mainChord:defState(2, noteWave, 0, 999, 1.0)

mainChord:addState(3) -- Wave + Skybox
mainChord:defState(3, noteWave, 0, 999, 1.0)
mainChord:defState(3, noteSkybox, 0, 999, 1.0)

-- Sync (120 BPM)
local song = {bpm = 120, bar = 4, cycle = 16, {bar = 4, cycle = 4}}
local sync = Sync:new(song)

-- ── Animation ────────────────────────────────────────────────────────────

local tn = bbfx.RootTimeNode.instance()
local animator = bbfx.Animator.instance()

local displacementVal = 0.1
local hueAccum = 0.0

-- Register chord state keys (F4-F7 to avoid conflict with F3 stats)
Keymap.register(1073741885, function() -- F4
    print("[set] Chord state 1: Perlin only")
    mainChord:send(1, sync, 0, "beat")
end)
Keymap.register(1073741886, function() -- F5
    print("[set] Chord state 2: Perlin + Wave")
    mainChord:send(2, sync, 0, "beat")
end)
Keymap.register(1073741887, function() -- F6
    print("[set] Chord state 3: Wave + Skybox")
    mainChord:send(3, sync, 0, "beat")
end)

-- Compositor toggle
Keymap.register(98, function() -- 'b'
    Compositor.toggle("Bloom")
end)

-- Particle toggle
Keymap.register(112, function() -- 'p'
    if aureola then
        aureolaVisible = not aureolaVisible
        aureola:setVisible(aureolaVisible)
        print("[set] Aureola: " .. (aureolaVisible and "ON" or "OFF"))
    end
end)

-- Joystick setup
local js1 = nil
local joyMgr = bbfx.InputManager.instance():getJoystick()
if joyMgr:getCount() > 0 then
    js1 = Joystick:open(0)
    js1:bind("axis", 0, function(val) displacementVal = val * 0.5 end)
    print("[set] Joystick 1 connected: axis 0 → Perlin displacement")
end

-- Main update loop
local mainNode = bbfx.LuaAnimationNode("setMain", function(self)
    local dtPort = self:getInput("dt")
    if not dtPort then return end
    local dt = dtPort:getValue()

    -- Rotation
    geoObj.node:yaw(Ogre.Radian(dt * 0.3))
    waveObj.node:yaw(Ogre.Radian(dt * 0.5))

    -- Perlin update
    perlin:renderOneFrame(dt)
    wave:renderOneFrame(dt)

    -- ColorShift cycling
    hueAccum = hueAccum + dt * 30.0
    colorShift:getInput("hue_shift"):setValue(hueAccum % 360.0)
    colorShift:update()

    -- Camera orbit
    azimuth = azimuth + dt * 0.15
    local cosEl = math.cos(elevation)
    camNode:setPosition(Ogre.Vector3(
        distance * cosEl * math.cos(azimuth),
        distance * math.sin(elevation),
        distance * cosEl * math.sin(azimuth)
    ))
    camNode:lookAt(Ogre.Vector3(0, 0, 0), 2)

    -- Keyboard controls
    Keymap.poll()

    if keyboard:wasKeyPressed(1073741906) then -- UP
        displacementVal = displacementVal + 0.02
        print("[set] displacement = " .. string.format("%.3f", displacementVal))
    end
    if keyboard:wasKeyPressed(1073741905) then -- DOWN
        displacementVal = math.max(0, displacementVal - 0.02)
        print("[set] displacement = " .. string.format("%.3f", displacementVal))
    end

    -- Poll joystick
    if js1 then js1:poll() end
end)
mainNode:addInput("dt")
animator:addNode(tn)
animator:addNode(mainNode)
animator:addPort(tn, "dt", mainNode, "dt")

-- Export DOT
animator:exportDOT("demo_set_graph.dot")
print("[set] Graph exported to demo_set_graph.dot")
print("[set] Ready. Press ESC to exit.")
