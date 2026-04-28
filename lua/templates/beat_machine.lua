-- Template: Beat Machine
-- Description: 4 effects tied to beat subdivisions
-- BPM: 128
-- Nodes: SceneObjectNode, PerlinFxNode, ParticleNode, ColorShiftNode, CameraNode, LightNode
return {
    name = "Beat Machine",
    bpm = 128,
    description = "4 presets on chord triggers",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(128) end
        dbg.create("SceneObjectNode", "mesh")
        dbg.create("PerlinFxNode", "perlin")
        dbg.create("ParticleNode", "particles")
        dbg.create("ColorShiftNode", "color")
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode", "light")
        dbg.set("perlin", "amplitude", 0.3)
        dbg.set("perlin", "frequency", 2.0)
        dbg.set("color", "hue_shift", 0.4)
        dbg.link("mesh", "entity", "perlin", "entity")
        dbg.link("mesh", "entity", "color", "entity")
    end
}
