-- demo_v25.lua — BBFx v2.5 "Animator Avancé" Complete Demo
-- Scene: Perlin geosphere + spline-animated particle + TextureSet cycling
-- Temporal chain: LFONode (sin 0.5Hz) → RampNode (rate=2) → perlinNode.displacement
-- Preset "PerlinPulse" instantiated via declarative style

package.path = "lua/?.lua;" .. package.path

require 'helpers'
require 'object'
require 'effect'
require 'animation'
require 'textureset'
require 'declarative'
require 'presets/perlin_pulse'

bbfx_globals()

print("=== BBFx v2.5 — Animator Avancé Demo ===")
print("")
print("Controls:")
print("  [  : LFO frequency -0.1 Hz")
print("  ]  : LFO frequency +0.1 Hz")
print("  P  : Play / Pause spline animation")
print("  R  : Reset LFO frequency to 0.5 Hz")
print("  N  : Next TextureSet preset")
print("  ESC: Quit")
print("")

-- ── Scene setup ────────────────────────────────────────────────────────────

Effect.ambient(0.4, 0.4, 0.4)
Effect.bg(Ogre.ColourValue(0.04, 0.04, 0.1))

local lightObj = Object:fromLight(Ogre.Light.LT_POINT, Ogre.ColourValue(1, 1, 1))
lightObj:setPosition(Ogre.Vector3(200, 300, 200))

local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode(UID("cam/"))
camNode:attachObject(cam)
camNode:setPosition(Ogre.Vector3(0, 0, 300))
camNode:lookAt(Ogre.Vector3(0, 0, 0), 2)

-- ── Geosphere with PerlinFxNode ────────────────────────────────────────────

local perlinFx = bbfx.PerlinFxNode("ogrehead.mesh", "v25_perlin")
perlinFx:enable()
local head = Object:fromMesh("v25_perlin")

-- ── Spline-animated particle (Compo pattern) ───────────────────────────────
-- Use a regular object if particle template is unavailable

local animTarget = nil
local ok, psys = pcall(function()
    return Object:fromPsys("particle/starwars")
end)
if ok and psys then
    animTarget = psys
    print("[demo_v25] Particle system loaded: particle/starwars")
else
    print("[demo_v25] Particle unavailable — spline animation skipped")
end

-- Spline animation on the animated target (only if particles loaded)
local splineAnim = nil
local animPlaying = false
if animTarget then
    splineAnim = Animation:new()
    splineAnim:addFrames(
        {length = 1, translate = Ogre.Vector3(0,   0,   0)},
        {length = 1, translate = Ogre.Vector3(60,  40,  0)},
        {length = 1, translate = Ogre.Vector3(100, 0,   50)},
        {length = 1, translate = Ogre.Vector3(60, -40,  0)},
        {length = 1, translate = Ogre.Vector3(0,   0,   0)}
    )
    splineAnim:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)
    splineAnim:bind(animTarget.animNode)
    splineAnim:play()
    animPlaying = true
end

-- ── TextureSet cycling ─────────────────────────────────────────────────────

local texPresets = {
    {gray='08b.jpg',           color='frac385-2.jpg',         rotate=true,  factor=-0.5},
    {gray='electric_nebula.png', color='subliminal_spectrum.jpg'},
    {gray='lace.jpg',          color='204-CMYK Fire.jpg',     rotate=true,  factor=-1.0},
    {gray='penrosef.jpg',      color='electric_nebula.jpg',   rotate=true,  factor=-3.0},
}
local texCycle = TextureCycle:new(texPresets)
local texSettings = {uscroll=1/60, vscroll=1/90, rspeed=1/150, sweepstate=1}
local texSet = TextureSet:new(texSettings, texCycle)
texSet:on()
print("[demo_v25] TextureCycle: " .. #texCycle.textures .. " presets, TextureSet active")

-- ── PerlinPulse preset + declarative graph ─────────────────────────────────
-- Build LFO → Ramp → perlinFx.displacement via declarative style

local handles = build({
    nodes = {
        {name="lfo",    type="LFO",      frequency=0.5, amplitude=30.0, waveform=0},
        {name="ramp",   type="Ramp",     rate=2.0},
        {name="perlin", type="existing", ref=perlinFx},
    },
    links = {
        {from="lfo.out",  to="ramp.target"},
        {from="ramp.out", to="perlin.displacement"},
    }
})

-- Also instantiate PerlinPulse preset (parallel path, doesn't override above wiring)
local pp = Preset:instantiate("PerlinPulse", {frequency=0.5, amplitude=20.0, rate=3.0})
print("[demo_v25] PerlinPulse preset instantiated (frequency=" ..
    pp:getOutput("value") .. " initial)")

-- Wire dt to temporal nodes
local animator = bbfx.Animator.instance()
local tn        = bbfx.RootTimeNode.instance()
animator:addNode(tn)
local lfo_raw  = handles.nodes.lfo._node
local ramp_raw = handles.nodes.ramp._node
animator:addPort(tn, "dt", lfo_raw,  "dt")
animator:addPort(tn, "dt", ramp_raw, "dt")
animator:addNode(perlinFx)

print("[demo_v25] Declarative graph: LFO(0.5Hz) → Ramp(rate=2) → PerlinFxNode.displacement")
print("[demo_v25] PerlinPulse preset: active (parallel LFO)")

-- ── Keyboard control node ─────────────────────────────────────────────────

local updateNode = bbfx.LuaAnimationNode(UID("v25update/"), function(self_node)
    local dtPort = self_node:getInput("dt")
    if not dtPort then return end
    local dt = dtPort:getValue()

    -- Rotation
    head.node:yaw(Ogre.Radian(dt * 0.4))

    -- [ → LFO freq -0.1
    if keyboard:wasKeyPressed(91) then
        local p = lfo_raw:getInput("frequency")
        if p then
            local f = math.max(0.05, p:getValue() - 0.1)
            p:setValue(f)
            print("[demo_v25] LFO frequency: " .. string.format("%.2f", f) .. " Hz")
        end
    end

    -- ] → LFO freq +0.1
    if keyboard:wasKeyPressed(93) then
        local p = lfo_raw:getInput("frequency")
        if p then
            local f = p:getValue() + 0.1
            p:setValue(f)
            print("[demo_v25] LFO frequency: " .. string.format("%.2f", f) .. " Hz")
        end
    end

    -- P → play/pause spline animation (if available)
    if splineAnim and keyboard:wasKeyPressed(112) then
        if animPlaying then
            splineAnim:stop()
            animPlaying = false
            print("[demo_v25] Spline animation: PAUSED")
        else
            splineAnim:play()
            animPlaying = true
            print("[demo_v25] Spline animation: PLAYING")
        end
    end

    -- R → reset LFO to 0.5 Hz
    if keyboard:wasKeyPressed(114) then
        local p = lfo_raw:getInput("frequency")
        if p then p:setValue(0.5) end
        print("[demo_v25] LFO frequency reset to 0.5 Hz")
    end

    -- N → next TextureSet preset
    if keyboard:wasKeyPressed(110) then
        local p = texCycle:next()
        if p then
            print("[demo_v25] TextureSet: " .. tostring(p.gray) .. " / " .. tostring(p.color))
        end
    end
end)
updateNode:addInput("dt")
animator:addNode(updateNode)
animator:addPort(tn, "dt", updateNode, "dt")

print("[demo_v25] Ready — press ESC to exit.")
