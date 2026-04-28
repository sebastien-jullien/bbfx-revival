-- Template: Ambient
-- Description: Slow organic visuals for ambient music
-- BPM: 70
-- Nodes: SceneObjectNode, PerlinFxNode, ColorShiftNode, CameraNode, LightNode
return {
    name = "Ambient",
    bpm = 70,
    description = "Slow organic visuals for ambient music",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(70) end

        dbg.create_with_param("SceneObjectNode", "mesh", "mesh_file", "geosphere8000.mesh")
        dbg.create("PerlinFxNode", "perlin")
        dbg.create("ColorShiftNode", "color")
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode", "light")
        dbg.set("perlin", "amplitude", 0.15)
        dbg.set("perlin", "frequency", 1.0)
        dbg.set("perlin", "speed", 0.3)
        dbg.set("color", "hue_shift", 0.1)
        dbg.set("color", "speed", 0.05)

        dbg.link("mesh", "entity", "perlin", "entity")
        dbg.link("mesh", "entity", "color", "entity")
    end
}
