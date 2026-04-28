-- Template: Hip-Hop
-- Description: Cool flowing visuals for hip-hop
-- BPM: 90
-- Nodes: SceneObjectNode, WaveVertexShader, ColorShiftNode, CameraNode, LightNode
return {
    name = "Hip-Hop",
    bpm = 90,
    description = "Cool flowing visuals for hip-hop",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(90) end

        dbg.create_with_param("SceneObjectNode", "mesh", "mesh_file", "torus.mesh")
        dbg.create("PerlinFxNode", "perlin")
        dbg.create("ColorShiftNode", "color")
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode", "light")
        dbg.set("perlin", "amplitude", 0.2)
        dbg.set("perlin", "frequency", 1.5)
        dbg.set("color", "hue_shift", 0.3)

        dbg.link("mesh", "entity", "perlin", "entity")
        dbg.link("mesh", "entity", "color", "entity")
    end
}
