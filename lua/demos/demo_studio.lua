-- demo_studio.lua
-- BBFx Studio v3.1 — Scene setup ONLY (no DAG)
-- The DAG (nodes, links, Lua code) comes from the .bbfx-project file.
-- This script only creates the OGRE 3D scene: mesh, camera, lights.

print("[demo_studio] Setting up OGRE scene...")

local engine = bbfx.Engine.instance()
local scene = engine:getSceneManager()

-- ── Scene setup ──────────────────────────────────────────────────────────────

-- Ambient + directional light
scene:setAmbientLight(ColourValue(0.3, 0.3, 0.35))

local light = scene:createLight("StudioLight")
light:setType(Light.LT_POINT)
light:setDiffuseColour(ColourValue(0.9, 0.85, 0.8))
light:setSpecularColour(ColourValue(1, 1, 1))
local lightNode = scene:getRootSceneNode():createChildSceneNode("StudioLightNode")
lightNode:setPosition(Vector3(50, 80, 120))
lightNode:attachObject(light)

-- Load ogrehead mesh
local headNode = scene:getRootSceneNode():createChildSceneNode("StudioHeadNode")
local ok, err = pcall(function()
    local entity = scene:createEntity("StudioHead", "ogrehead.mesh")
    headNode:attachObject(entity)
    print("[demo_studio] ogrehead.mesh loaded")
end)
if not ok then
    print("[demo_studio] Mesh not available: " .. tostring(err))
end

-- Camera
local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode("CameraNode")
camNode:setPosition(Vector3(0, 25, 150))
camNode:lookAt(Vector3(0, 0, 0), 1)  -- TS_WORLD
camNode:attachObject(cam)

-- Make headNode accessible globally for Lua animation nodes
_G.headNode = headNode

print("[demo_studio] Scene ready. DAG will be loaded from project file.")
