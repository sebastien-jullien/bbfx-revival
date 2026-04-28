-- Template: BonneBalle Basic
-- Description: Geosphere + Perlin deformation + color shift + orbit camera + light
-- BPM: 120
-- Nodes: SceneObjectNode, PerlinFxNode, ColorShiftNode, CameraNode, LightNode
return {
    name = "BonneBalle Basic",
    bpm = 120,
    description = "Geosphere + Perlin + orbit camera",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end

        dbg.create("SceneObjectNode", "mesh")
        dbg.create("PerlinFxNode", "perlin")
        dbg.create("ColorShiftNode", "color")
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode", "light")
        dbg.set("perlin", "amplitude", 0.3)
        dbg.set("perlin", "frequency", 2.0)
        dbg.set("color", "hue_shift", 0.5)

        dbg.link("mesh", "entity", "perlin", "entity")
        dbg.link("mesh", "entity", "color", "entity")
    end
}
