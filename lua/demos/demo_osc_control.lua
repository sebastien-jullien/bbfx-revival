-- BBFx v3.4 Demo: OSC Control
-- Receives OSC messages on port 9000 and maps them to the DAG.
-- Send from TouchOSC, Max/MSP, or any OSC client:
--   /bbfx/amplitude <float 0-1>

print("[demo_osc_control] Setting up OSC-controlled scene...")

dbg.create("SceneObjectNode", "mesh")
dbg.create("PerlinFxNode", "perlin")
dbg.create("OscInputNode", "osc_in")

local frame = 0
local animator = bbfx.Animator.instance()
local tickNode = bbfx.LuaAnimationNode("_demo_osc_tick", function(self)
    frame = frame + 1
    if frame == 10 then
        dbg.link("mesh", "entity", "perlin", "entity")
        dbg.link("osc_in", "value1", "perlin", "displacement")
        dbg.set_param("osc_in", "port", "9000")
        dbg.set_param("osc_in", "address", "/bbfx/*")
        print("[demo_osc_control] Ready! Send OSC to port 9000, address /bbfx/amplitude")
    end
end)
tickNode:addInput("dt")
animator:addNode(tickNode)
local rootTime = bbfx.RootTimeNode.instance()
if rootTime then animator:addPort(rootTime, "dt", tickNode, "dt") end
