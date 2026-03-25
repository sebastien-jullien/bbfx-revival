-- demo.lua — BBFx unified demo
--
-- Usage:
--   bbfx.exe lua/demo.lua              → mode minimal (ogrehead + rotation)
--   bbfx.exe lua/demo.lua minimal      → idem
--   bbfx.exe lua/demo.lua perlin       → ogrehead + Perlin noise deformation

local mode = (arg and arg[2]) or "minimal"
print("[demo] mode: " .. mode)

-- ── Scene setup (shared) ────────────────────────────────────────────────────

local engine = bbfx.Engine.instance()
local scene = engine:getSceneManager()

scene:setAmbientLight(Ogre.ColourValue(0.5, 0.5, 0.5))

-- Light
local light = scene:createLight("MainLight")
light:setType(Ogre.Light.LT_POINT)
light:setDiffuseColour(Ogre.ColourValue(1, 1, 1))
local lightNode = scene:getRootSceneNode():createChildSceneNode("LightNode")
lightNode:setPosition(Ogre.Vector3(100, 200, 300))
lightNode:attachObject(light)

-- Camera
local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode("CamNode")
camNode:attachObject(cam)
camNode:setPosition(Ogre.Vector3(0, 10, 200))
camNode:lookAt(Ogre.Vector3(0, 0, 0), 2) -- TS_WORLD

-- ── Mode-specific setup ─────────────────────────────────────────────────────

local headNode = scene:getRootSceneNode():createChildSceneNode("HeadNode")
local rotSpeed = 0.5

if mode == "perlin" then
    local perlin = bbfx.PerlinVertexShader("ogrehead.mesh", "ogrehead_perlin")
    perlin:enable()
    local entity = scene:createEntity("Head", "ogrehead_perlin")
    headNode:attachObject(entity)
    rotSpeed = 0.3
    print("[demo] Perlin noise deformation active on ogrehead.mesh")
else
    local entity = scene:createEntity("Head", "ogrehead.mesh")
    headNode:attachObject(entity)
    print("[demo] ogrehead.mesh with rotation")
end

-- ── Animation (shared) ──────────────────────────────────────────────────────

local tn = bbfx.RootTimeNode.instance()
local animator = bbfx.Animator.instance()

local rotNode = bbfx.LuaAnimationNode("rotate", function(self)
    local p = self:getInput("dt")
    if p then
        headNode:yaw(Ogre.Radian(p:getValue() * rotSpeed))
    end
end)
rotNode:addInput("dt")
animator:addNode(tn)
animator:addNode(rotNode)
animator:addPort(tn, "dt", rotNode, "dt")

print("[demo] Press ESC to exit")
