-- demo_studio.lua
-- BBFx Studio v3.2 — Minimal OGRE scene setup
-- Only creates camera and ambient light. All content (meshes, lights, effects)
-- is created by the project template as regular DAG nodes.

print("[demo_studio] Setting up minimal OGRE scene...")

local engine = bbfx.Engine.instance()
local scene = engine:getSceneManager()

-- Ambient light (scene-level, not a node)
scene:setAmbientLight(ColourValue(0.3, 0.3, 0.35))

-- Camera (required for rendering)
local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode("CameraNode")
camNode:setPosition(Vector3(0, 25, 150))
camNode:lookAt(Vector3(0, 0, 0), 1)  -- TS_WORLD
camNode:attachObject(cam)

print("[demo_studio] Minimal scene ready. Content will be loaded from project.")
