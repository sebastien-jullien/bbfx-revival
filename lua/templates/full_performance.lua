-- Template: Full Performance
-- Description: Complete VJ set with all node types
-- BPM: 128
-- Nodes: SceneObjectNode, PerlinFxNode, ColorShiftNode, ParticleNode, PostProcessNode, CameraNode, LightNode, ShaderFxNode
return {
    name = "Full Performance",
    bpm = 128,
    description = "Complete VJ set pre-configured",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(128) end
        dbg.create("SceneObjectNode", "mesh")
        dbg.create("PerlinFxNode", "perlin")
        dbg.create("ColorShiftNode", "color")
        dbg.create("ParticleNode", "particles")
        dbg.create("PostProcessNode", "bloom")
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode", "light1")
        dbg.create("LightNode", "light2")
        dbg.set("perlin", "amplitude", 0.3)
        dbg.set("perlin", "frequency", 2.0)
        dbg.set("color", "hue_shift", 0.5)
        dbg.link("mesh", "entity", "perlin", "entity")
        dbg.link("mesh", "entity", "color", "entity")
    end
}
