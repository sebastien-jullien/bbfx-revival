-- Template: Drum & Bass
-- Description: Fast nervous visuals with chromatic aberration
-- BPM: 172
-- Nodes: SceneObjectNode, PerlinFxNode, ParticleNode, ColorShiftNode, PostProcessNode, CameraNode, LightNode
return {
    name = "Drum & Bass",
    bpm = 172,
    description = "Fast nervous visuals for D&B",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(172) end
        dbg.create("SceneObjectNode", "mesh")
        dbg.create("PerlinFxNode", "perlin")
        dbg.create("ParticleNode", "particles")
        dbg.create("ColorShiftNode", "color")
        dbg.create("PostProcessNode", "chromatic")
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode", "light")
        dbg.set("perlin", "amplitude", 0.35)
        dbg.set("perlin", "frequency", 4.0)
        dbg.set("perlin", "speed", 3.0)
        dbg.set("color", "hue_shift", 0.7)
        dbg.link("mesh", "entity", "perlin", "entity")
        dbg.link("mesh", "entity", "color", "entity")
    end
}
