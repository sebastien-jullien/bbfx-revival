-- demo_studio.lua
-- BBFx Studio v3.2 — Minimal OGRE scene setup
-- Only creates camera and ambient light. All content (meshes, lights, effects)
-- is created by the project template as regular DAG nodes.

print("[demo_studio] Setting up minimal OGRE scene...")

local engine = bbfx.Engine.instance()
local scene = engine:getSceneManager()

-- Ambient light (scene-level, not a node)
scene:setAmbientLight(ColourValue(0.3, 0.3, 0.35))

-- Camera: managed by ViewportCameraController (default orbit: distance=25, pitch=15°).
-- No manual positioning needed — the controller handles everything.

print("[demo_studio] Minimal scene ready. Content will be loaded from project.")
