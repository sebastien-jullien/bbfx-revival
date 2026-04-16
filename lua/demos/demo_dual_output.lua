-- BBFx v3.4 Demo: Dual Output
-- Opens a secondary output window for projector display.

print("[demo_dual_output] Opening output window...")

local frame = 0
local animator = bbfx.Animator.instance()
local tickNode = bbfx.LuaAnimationNode("_demo_output_tick", function(self)
    frame = frame + 1
    if frame == 10 then
        if dbg.output_open then
            dbg.output_open(1280, 720)
            print("[demo_dual_output] Output window opened at 1280x720")
            print("[demo_dual_output] Use View > Output to control resolution/fullscreen")
        else
            print("[demo_dual_output] dbg.output_open not available")
        end
    end
end)
tickNode:addInput("dt")
animator:addNode(tickNode)
local rootTime = bbfx.RootTimeNode.instance()
if rootTime then animator:addPort(rootTime, "dt", tickNode, "dt") end
