-- Template: Techno
-- Description: Multi-mesh aggressive visuals for techno
-- BPM: 135
-- Nodes: 3x SceneObjectNode, PerlinFxNode, ColorShiftNode, CameraNode, LightNode
return {
    name = "Techno",
    bpm = 135,
    description = "Hard geometric visuals for techno",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(135) end
        dbg.create_with_param("SceneObjectNode", "mesh1", "mesh_file", "cube.mesh")
        dbg.create_with_param("SceneObjectNode", "mesh2", "mesh_file", "torus.mesh")
        dbg.create("SceneObjectNode", "mesh3")
        dbg.create("PerlinFxNode", "perlin")
        dbg.create("ColorShiftNode", "color")
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode", "light")
        dbg.set("perlin", "amplitude", 0.4)
        dbg.set("perlin", "frequency", 3.0)
        dbg.set("perlin", "speed", 2.0)
        dbg.set("color", "hue_shift", 0.8)
        dbg.link("mesh1", "entity", "perlin", "entity")
        dbg.link("mesh1", "entity", "color", "entity")
    end
}
