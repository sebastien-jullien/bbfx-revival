-- demo_particles.lua — BBFx v2.3 Particle Systems Demo

package.path = "lua/?.lua;" .. package.path

require 'helpers'
require 'object'
require 'effect'
require 'compositors'

bbfx_globals()

print("[demo_particles] BBFx Particle + Compositor Demo")
print("  1: Toggle Aureola particles")
print("  2: Toggle PurpleFountain particles")
print("  3: Toggle Rain particles")
print("  B: Toggle Bloom compositor")
print("  N: Toggle B&W compositor")
print("  ESC: quit")

-- Scene setup
Effect.ambient(0.2, 0.2, 0.3)
Effect.bg(Ogre.ColourValue(0.1, 0.1, 0.2))

-- Light
local lightObj = Object:fromLight(Ogre.Light.LT_POINT, Ogre.ColourValue(1, 1, 1))
lightObj:setPosition(Ogre.Vector3(100, 200, 100))

-- Base mesh for reference
local head = Object:fromMesh("ogrehead.mesh")
head:setPosition(Ogre.Vector3(0, 0, 0))

-- Camera
local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode(UID("cam/"))
camNode:attachObject(cam)
camNode:setPosition(Ogre.Vector3(0, 50, 300))
camNode:lookAt(Ogre.Vector3(0, 0, 0), 2)

-- Particle systems
local particles = {}
local particleNames = {"Examples/Aureola", "Examples/PurpleFountain", "Examples/Rain"}
local particlePositions = {
    Ogre.Vector3(  0,   0, 0),   -- Aureola: autour de la tête
    Ogre.Vector3(-120,  0, 0),   -- PurpleFountain: à gauche
    Ogre.Vector3( 120, 200, 0),  -- Rain: en haut à droite
}
local particleVisible = {true, true, true}

for i, name in ipairs(particleNames) do
    local ok, obj = pcall(function()
        return Object:fromPsys(name)
    end)
    if ok and obj then
        obj:setPosition(particlePositions[i])
        obj.movable:setEmitting(true)
        obj.movable:fastForward(2.0, 0.1)
        particles[i] = {system = obj.movable, node = obj.node, obj = obj, name = name}
        print("[demo_particles] Loaded: " .. name)
    else
        print("[demo_particles] Could not load: " .. name .. " (template may not exist)")
    end
end

-- Animation
local tn = bbfx.RootTimeNode.instance()
local animator = bbfx.Animator.instance()

local rotNode = bbfx.LuaAnimationNode("particleUpdate", function(self)
    local dtPort = self:getInput("dt")
    if not dtPort then return end
    local dt = dtPort:getValue()

    head.node:yaw(Ogre.Radian(dt * 0.3))

    -- Keyboard toggle particles
    if keyboard:wasKeyPressed(49) then -- '1'
        if particles[1] then
            particleVisible[1] = not particleVisible[1]
            particles[1].system:setVisible(particleVisible[1])
            print("[demo_particles] Aureola: " .. (particleVisible[1] and "ON" or "OFF"))
        end
    end
    if keyboard:wasKeyPressed(50) then -- '2'
        if particles[2] then
            particleVisible[2] = not particleVisible[2]
            particles[2].system:setVisible(particleVisible[2])
            print("[demo_particles] PurpleFountain: " .. (particleVisible[2] and "ON" or "OFF"))
        end
    end
    if keyboard:wasKeyPressed(51) then -- '3'
        if particles[3] then
            particleVisible[3] = not particleVisible[3]
            particles[3].system:setVisible(particleVisible[3])
            print("[demo_particles] Rain: " .. (particleVisible[3] and "ON" or "OFF"))
        end
    end

    -- Compositor toggles
    if keyboard:wasKeyPressed(98) then -- 'b'
        Compositor.toggle("Bloom")
    end
    if keyboard:wasKeyPressed(110) then -- 'n'
        Compositor.toggle("B&W")
    end
end)
rotNode:addInput("dt")
animator:addNode(tn)
animator:addNode(rotNode)
animator:addPort(tn, "dt", rotNode, "dt")

print("[demo_particles] Press ESC to exit")
