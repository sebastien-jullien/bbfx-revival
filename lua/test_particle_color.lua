-- test_particle_color.lua — automated particle colour test
-- Creates a ParticleNode, sets RED colour, takes screenshot

print("[TEST] Setting up particle color test...")

local engine = bbfx.Engine.instance()
local scene = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.0, 0.0, 0.0))

local frame = 0
local created = false
local templateSet = false
local colorSet = false

local function tick()
    frame = frame + 1

    if frame == 3 and not created then
        print("[TEST] Creating ParticleNode...")
        dbg.create("ParticleNode", "tp")
        created = true
    end

    if frame == 8 and not templateSet then
        -- Set template after node is created (async creation)
        print("[TEST] Setting template to BBFx/ParticleTunnel...")
        dbg.set_param("tp", "template", "BBFx/ParticleTunnel")
        templateSet = true
    end

    if frame == 15 and not colorSet then
        -- Set colour to bright RED
        print("[TEST] Setting color to RED (r=1 g=0 b=0 a=1)...")
        dbg.set("tp", "color.r", 1.0)
        dbg.set("tp", "color.g", 0.0)
        dbg.set("tp", "color.b", 0.0)
        dbg.set("tp", "color.a", 1.0)
        colorSet = true
    end

    if frame == 90 then
        print("[TEST] Taking screenshot at frame 90...")
        dbg.screenshot("lua/particle_color_test.png")
        print("[TEST] Screenshot saved")
    end

    if frame == 95 then
        print("[TEST] Test complete. Exiting.")
        os.exit(0)
    end
end

local animator = bbfx.Animator.instance()
local testNode = bbfx.LuaAnimationNode("_particle_color_test", function(self)
    tick()
end)
testNode:addInput("dt")
animator:addNode(testNode)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", testNode, "dt")
end
