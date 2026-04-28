-- Template: Dubstep
-- Description: Heavy bass-driven visuals with VHS effect
-- BPM: 140
-- Nodes: SceneObjectNode, PerlinFxNode, ColorShiftNode, PostProcessNode, CameraNode, LightNode
return {
    name = "Dubstep",
    bpm = 140,
    description = "Heavy bass drop visuals for dubstep",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(140) end
        dbg.create("SceneObjectNode", "mesh")
        dbg.create("PerlinFxNode", "perlin")
        dbg.create("ColorShiftNode", "color")
        dbg.create("PostProcessNode", "vhs")
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode", "light")
        dbg.set("perlin", "amplitude", 0.5)
        dbg.set("perlin", "frequency", 2.5)
        dbg.set("color", "hue_shift", 0.6)
        dbg.link("mesh", "entity", "perlin", "entity")
        dbg.link("mesh", "entity", "color", "entity")
    end
}
