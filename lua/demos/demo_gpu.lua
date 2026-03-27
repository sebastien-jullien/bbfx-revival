-- demo_gpu.lua — BBFx v2.8 "GPU & Shaders" Demo
-- Perlin GPU shader + audio reactive + profiling overlay

package.path = "lua/?.lua;" .. package.path

require 'helpers'
require 'object'
require 'effect'
require 'shader'
require 'audio'
require 'hud'
require 'profiler'
require 'console'

bbfx_globals()

print("=== BBFx v2.8 — GPU & Shaders Demo ===")
print("")
print("  Perlin noise runs on GPU via GLSL vertex shader.")
print("  Audio reactive: microphone RMS modulates displacement.")
print("")
print("Controls:")
print("  H  : Toggle audio HUD")
print("  P  : Toggle profiler overlay")
print("  [  : Decrease displacement")
print("  ]  : Increase displacement")
print("  ESC: Quit")
print("")

-- Scene setup
Effect.ambient(0.4, 0.4, 0.4)
Effect.bg(Ogre.ColourValue(0.03, 0.03, 0.08))

local lightObj = Object:fromLight(Ogre.Light.LT_POINT, Ogre.ColourValue(1, 1, 1))
lightObj:setPosition(Ogre.Vector3(200, 300, 200))

local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode(UID("cam/"))
camNode:attachObject(cam)
camNode:setPosition(Ogre.Vector3(0, 0, 300))
camNode:lookAt(Ogre.Vector3(0, 0, 0), 2)

-- Geosphere with GPU Perlin shader (falls back to CPU PerlinFxNode if GPU shader fails)
local perlinFx = bbfx.PerlinFxNode("ogrehead.mesh", "gpu_perlin")
perlinFx:enable()
local head = Object:fromMesh("gpu_perlin")

local gpuFx = nil
local ok, err = pcall(function()
    gpuFx = Shader:load("shaders/perlin_deform.glsl", {
        mesh = head.movable,
        displacement = 15.0,
        frequency = 3.0,
        speed = 1.5
    })
end)
if not ok then
    print("[demo_gpu] GPU shader failed, using CPU PerlinFxNode: " .. tostring(err))
end

-- Audio chain
local audioInst = Audio:start()
_G._bbfx_audio = audioInst

-- HUD
local hudInst = HUD:new()
hudInst:show()

-- Wire audio RMS to GPU displacement
local animator = bbfx.Animator.instance()
local tn = bbfx.RootTimeNode.instance()

local baseDisplacement = 15.0

local updateNode = bbfx.LuaAnimationNode(UID("gpuDemo/"), function(self)
    local dtPort = self:getInput("dt")
    if not dtPort then return end
    local dt = dtPort:getValue()

    -- Rotate mesh
    head.node:yaw(Ogre.Radian(dt * 0.3))

    -- Audio → displacement modulation (GPU shader or CPU fallback)
    local rms = audioInst:getRMS()
    if gpuFx then
        gpuFx:setUniform("displacement", baseDisplacement + rms * 60.0)
    else
        local dispPort = perlinFx:getInput("displacement")
        if dispPort then dispPort:setValue(baseDisplacement + rms * 60.0) end
    end

    -- Update HUD
    hudInst:update(audioInst)

    -- Keyboard controls
    if keyboard:wasKeyPressed(104) then hudInst:toggle() end  -- H
    if keyboard:wasKeyPressed(112) then perf() end             -- P
    if keyboard:wasKeyPressed(91) then                          -- [
        baseDisplacement = math.max(1.0, baseDisplacement - 2.0)
        print("[demo_gpu] Displacement: " .. baseDisplacement)
    end
    if keyboard:wasKeyPressed(93) then                          -- ]
        baseDisplacement = baseDisplacement + 2.0
        print("[demo_gpu] Displacement: " .. baseDisplacement)
    end
end)
updateNode:addInput("dt")
animator:addNode(updateNode)
animator:addPort(tn, "dt", updateNode, "dt")

print("[demo_gpu] Ready — GPU Perlin shader active")
print("bbfx> ")
