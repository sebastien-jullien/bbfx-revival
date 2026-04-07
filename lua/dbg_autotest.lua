-- Auto-run debugger test suite after a short delay
-- This script is loaded by the Studio at startup
print("[dbg_autotest] Scheduling test suite...")

-- We need to wait a few frames for everything to initialize
local frame = 0
local function tick()
    frame = frame + 1
    if frame == 5 then
        print("[dbg_autotest] Running dbg.test()...")
        dbg.test()
        print("[dbg_autotest] Done.")
    end
end

-- Register a frame callback via the animation system
local animator = bbfx.Animator.instance()
local testNode = bbfx.LuaAnimationNode("_dbg_autotest", function(self)
    tick()
end)
testNode:addInput("dt")
animator:addNode(testNode)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", testNode, "dt")
end
