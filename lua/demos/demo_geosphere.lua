-- demo_geosphere.lua — BBFx v2.3 Geosphere Perlin Demo
-- The signature BBFx effect: geosphere with Perlin noise deformation

package.path = "lua/?.lua;" .. package.path

require 'helpers'
require 'object'
require 'effect'
require 'camera'
require 'joystick_mapping'
require 'keymap'

bbfx_globals()

print("[demo_geosphere] BBFx Geosphere Perlin Demo")
print("  Up/Down: adjust Perlin displacement")
print("  Left/Right: adjust rotation speed")
print("  Joystick axis 1: Perlin displacement")
print("  ESC: quit")

-- Scene setup
Effect.ambient(0.3, 0.3, 0.3)
Effect.bg(Ogre.ColourValue(0.05, 0.05, 0.15))

-- Light
local lightObj = Object:fromLight(Ogre.Light.LT_POINT, Ogre.ColourValue(1, 1, 1))
lightObj:setPosition(Ogre.Vector3(200, 300, 200))

-- Geosphere with Perlin (use ogrehead as stand-in if geosphere not available)
local meshName = "ogrehead.mesh"
local perlin = bbfx.PerlinVertexShader(meshName, "geosphere_perlin")
perlin:enable()

local geoObj = Object:fromMesh("geosphere_perlin")
geoObj:setPosition(Ogre.Vector3(0, 0, 0))

-- Camera with SphereTrack
local cam = scene:getCamera("MainCamera")
local st = SphereTrack:new(cam)
st.distance = 200
st.elevation = 0.3

-- Animation
local tn = bbfx.RootTimeNode.instance()
local animator = bbfx.Animator.instance()

local displacementVal = 0.1
local rotSpeed = 0.3

-- Try to open joystick
local js = nil
local joyMgr = bbfx.InputManager.instance():getJoystick()
if joyMgr:getCount() > 0 then
    js = Joystick:open(0)
    js:bind("axis", 1, function(val)
        -- val is 0..1, center ~0.5; deadzone prevents overriding keyboard at rest
        if math.abs(val - 0.5) > 0.1 then
            displacementVal = val * 0.5
        end
    end)
    print("[demo_geosphere] Joystick detected")
end

-- Main update node
local updateNode = bbfx.LuaAnimationNode("geoUpdate", function(self)
    local dtPort = self:getInput("dt")
    if not dtPort then return end
    local dt = dtPort:getValue()

    -- Rotation
    geoObj.node:yaw(Ogre.Radian(dt * rotSpeed))

    -- Perlin update
    perlin:setDisplacement(displacementVal)
    perlin:renderOneFrame(dt)

    -- SphereTrack auto-rotate
    st.azimuth = st.azimuth + dt * 0.2

    -- Keyboard controls
    if keyboard:wasKeyPressed(1073741906) then -- UP
        displacementVal = displacementVal + 0.02
        print("[demo_geosphere] displacement = " .. displacementVal)
    end
    if keyboard:wasKeyPressed(1073741905) then -- DOWN
        displacementVal = math.max(0, displacementVal - 0.02)
        print("[demo_geosphere] displacement = " .. displacementVal)
    end

    -- Joystick control
    if js then js:poll() end

    -- Update SphereTrack camera position
    local cosEl = math.cos(st.elevation)
    local x = st.distance * cosEl * math.cos(st.azimuth)
    local y = st.distance * math.sin(st.elevation)
    local z = st.distance * cosEl * math.sin(st.azimuth)
    st.camNode:setPosition(Ogre.Vector3(x, y, z))
    st.camNode:lookAt(Ogre.Vector3(0, 0, 0), 2)
end)
updateNode:addInput("dt")
animator:addNode(tn)
animator:addNode(updateNode)
animator:addPort(tn, "dt", updateNode, "dt")

print("[demo_geosphere] Press ESC to exit")
