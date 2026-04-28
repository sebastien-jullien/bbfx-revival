-- test_particle_link.lua — definitive particle colour test
-- Simple: create, set colour via dbg.set, screenshot. No fancy stuff.

print("[TEST] Particle colour test (material tint)")

-- Match test_particle_color.lua setup
local engine = bbfx.Engine.instance()
local scene = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.0, 0.0, 0.0))

local frame = 0

local function tick()
    frame = frame + 1

    if frame == 3 then
        dbg.create("ParticleNode", "tp")
        print("[TEST] Created ParticleNode 'tp'")
    end

    if frame == 8 then
        dbg.set_param("tp", "template", "BBFx/ParticleTunnel")
        print("[TEST] Template set to BBFx/ParticleTunnel")
    end

    if frame == 15 then
        -- Set RED
        dbg.set("tp", "color.r", 1.0)
        dbg.set("tp", "color.g", 0.0)
        dbg.set("tp", "color.b", 0.0)
        dbg.set("tp", "color.a", 1.0)
        print("[TEST] Colour set to RED (r=1 g=0 b=0 a=1)")
    end

    if frame == 120 then
        print("[TEST] Taking screenshot at frame 120...")
        dbg.screenshot("lua/particle_red_test.png")
        print("[TEST] Screenshot saved")
    end

    if frame == 125 then
        print("[TEST] Done!")
        os.exit(0)
    end
end

local animator = bbfx.Animator.instance()
local testNode = bbfx.LuaAnimationNode("_ptcl_test", function(self)
    tick()
end)
testNode:addInput("dt")
animator:addNode(testNode)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", testNode, "dt")
end
