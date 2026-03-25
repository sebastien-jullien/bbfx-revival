-- bbfx_minimal.lua — Minimal BBFx scene with rotating node
-- Demonstrates: Engine, SceneManager, SceneNode, Light, Camera, LuaAnimationNode

local engine = bbfx.Engine.instance()
local scene = engine:getSceneManager()

-- Configure scene
scene:setAmbientLight(ColourValue(0.5, 0.5, 0.5))

-- Create a light
local light = scene:createLight("MainLight")
light:setType(Light.LT_POINT)
light:setDiffuseColour(ColourValue(1, 1, 1))
local lightNode = scene:getRootSceneNode():createChildSceneNode("LightNode")
lightNode:setPosition(Vector3(20, 40, 50))
lightNode:attachObject(light)

-- Create a visible node (will show mesh when resources are configured)
local headNode = scene:getRootSceneNode():createChildSceneNode("HeadNode")

-- Try loading ogrehead.mesh if available
local ok, err = pcall(function()
    local entity = scene:createEntity("Head", "ogrehead.mesh")
    headNode:attachObject(entity)
    print("ogrehead.mesh loaded successfully")
end)
if not ok then
    print("ogrehead.mesh not available (expected in minimal mode): " .. tostring(err))
end

-- Position camera
local cam = scene:getCamera("MainCamera")
cam:setPosition(Vector3(0, 0, 80))
cam:lookAt(Vector3.ZERO)

-- Create rotation animation via LuaAnimationNode
local animator = bbfx.Animator.instance()

local rotNode = bbfx.LuaAnimationNode("rotate", function(node)
    headNode:yaw(Radian(0.016))
end)

animator:addNode(rotNode)

print("bbfx_minimal.lua loaded — scene initialized")
