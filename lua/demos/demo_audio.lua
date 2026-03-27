-- demo_audio.lua — BBFx v2.7 "Audio Réactif" Demo
-- The Saint Graal: visuals react to music in real-time

package.path = "lua/?.lua;" .. package.path

require 'helpers'
require 'object'
require 'effect'
require 'temporal_nodes'
require 'declarative'
require 'audio'
require 'hud'
require 'console'

bbfx_globals()

print("=== BBFx v2.7 — Audio Réactif Demo ===")
print("")
print("  The visuals react to your microphone input.")
print("  Play music near your mic to see the effects.")
print("")
print("Controls:")
print("  H  : Toggle HUD (BPM + frequency bands)")
print("  [  : Decrease LFO frequency")
print("  ]  : Increase LFO frequency")
print("  ESC: Quit")
print("")

-- Scene setup
Effect.ambient(0.3, 0.3, 0.3)
Effect.bg(Ogre.ColourValue(0.03, 0.03, 0.08))

local lightObj = Object:fromLight(Ogre.Light.LT_POINT, Ogre.ColourValue(1, 1, 1))
lightObj:setPosition(Ogre.Vector3(200, 300, 200))

local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode(UID("cam/"))
camNode:attachObject(cam)
camNode:setPosition(Ogre.Vector3(0, 0, 300))
camNode:lookAt(Ogre.Vector3(0, 0, 0), 2)

-- Geosphere with Perlin
local head = Object:fromMesh("ogrehead.mesh")
local perlinFx = bbfx.PerlinFxNode("ogrehead.mesh", "audio_perlin")

-- Audio chain
local audioInst = Audio:start()
_G._bbfx_audio = audioInst  -- expose for REPL audio() command

-- HUD
local hudInst = HUD:new()
hudInst:show()

-- Declarative graph: LFO base + audio RMS modulation
local handles = build({
    nodes = {
        {name="lfo",    type="LFO",      frequency=0.5, amplitude=10.0, waveform=0},
        {name="ramp",   type="Ramp",     rate=5.0},
        {name="perlin", type="existing", ref=perlinFx},
    },
    links = {
        {from="lfo.out",  to="ramp.target"},
        {from="ramp.out", to="perlin.displacement"},
    }
})

local animator = bbfx.Animator.instance()
local tn = bbfx.RootTimeNode.instance()
local lfo_raw  = handles.nodes.lfo._node
local ramp_raw = handles.nodes.ramp._node
animator:addPort(tn, "dt", lfo_raw,  "dt")
animator:addPort(tn, "dt", ramp_raw, "dt")
animator:addNode(perlinFx)

-- Main update node: modulate effects with audio
local updateNode = bbfx.LuaAnimationNode(UID("audioDemo/"), function(self)
    local dtPort = self:getInput("dt")
    if not dtPort then return end
    local dt = dtPort:getValue()

    -- Rotate mesh
    head.node:yaw(Ogre.Radian(dt * 0.3))

    -- Audio-reactive: RMS modulates LFO amplitude
    local rms = audioInst:getRMS()
    local ampPort = lfo_raw:getInput("amplitude")
    if ampPort then
        ampPort:setValue(10.0 + rms * 80.0)  -- 10..90 based on audio level
    end

    -- Beat → camera shake
    local beatPort = audioInst.beat:getOutput("beat")
    if beatPort and beatPort:getValue() > 0.5 then
        local shake = (math.random() - 0.5) * 5.0
        camNode:setPosition(Ogre.Vector3(shake, shake * 0.5, 300))
    else
        camNode:setPosition(Ogre.Vector3(0, 0, 300))
    end

    -- Update HUD
    hudInst:update(audioInst)

    -- Keyboard
    if keyboard:wasKeyPressed(104) then -- 'h'
        hudInst:toggle()
    end
    if keyboard:wasKeyPressed(91) then -- '['
        local p = lfo_raw:getInput("frequency")
        if p then p:setValue(math.max(0.05, p:getValue() - 0.1)) end
    end
    if keyboard:wasKeyPressed(93) then -- ']'
        local p = lfo_raw:getInput("frequency")
        if p then p:setValue(p:getValue() + 0.1) end
    end
end)
updateNode:addInput("dt")
animator:addNode(updateNode)
animator:addPort(tn, "dt", updateNode, "dt")

print("[demo_audio] Ready — play music near your mic!")
print("bbfx> ")
