-- BBFx v3.3 Demo: MIDI Live Performance
-- Demonstrates MIDI input controlling the DAG.
-- Works with any MIDI controller or via virtual device (dbg.midi_inject).

print("[demo_midi_live] Setting up MIDI-driven scene...")

-- Create a SceneObjectNode + PerlinFx
dbg.create("SceneObjectNode", "mesh")
dbg.create("PerlinFxNode", "perlin")
dbg.create("MidiInputNode", "midi_in")

-- Wait for nodes to be created
local frame = 0
local animator = bbfx.Animator.instance()
local tickNode = bbfx.LuaAnimationNode("_demo_midi_tick", function(self)
    frame = frame + 1
    if frame == 10 then
        -- Link MIDI CC1 to Perlin amplitude
        dbg.link("mesh", "entity", "perlin", "entity")
        dbg.link("midi_in", "cc1", "perlin", "amplitude")
        dbg.set_param("midi_in", "cc1_number", "1")
        print("[demo_midi_live] Ready! Move MIDI CC#1 to control Perlin amplitude")
        print("[demo_midi_live] Or use: dbg.midi_inject(1, 0xB0, 1, 64)")
    end
end)
tickNode:addInput("dt")
animator:addNode(tickNode)
local rootTime = bbfx.RootTimeNode.instance()
if rootTime then animator:addPort(rootTime, "dt", tickNode, "dt") end
