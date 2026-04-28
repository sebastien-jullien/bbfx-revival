-- Template: House
-- Description: Warm pulsing visuals for house music
-- BPM: 124
-- Nodes: SceneObjectNode, PerlinFxNode, ColorShiftNode, PostProcessNode(Bloom), CameraNode, LightNode
return {
    name = "House",
    bpm = 124,
    description = "Warm pulsing visuals for house music",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(124) end

        dbg.create("SceneObjectNode", "mesh")
        dbg.create("PerlinFxNode", "perlin")
        dbg.create("ColorShiftNode", "color")
        dbg.create("PostProcessNode", "bloom")
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode", "light")
        dbg.set("perlin", "amplitude", 0.25)
        dbg.set("perlin", "frequency", 2.0)
        dbg.set("color", "hue_shift", 0.2)
        dbg.set("color", "saturation", 0.3)

        dbg.link("mesh", "entity", "perlin", "entity")
        dbg.link("mesh", "entity", "color", "entity")
    end
}
