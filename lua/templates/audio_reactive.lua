-- Template: Audio Reactive
-- Description: Microphone input drives visuals
-- BPM: 0
-- Nodes: SceneObjectNode, PerlinFxNode, ColorShiftNode, ParticleNode, CameraNode, LightNode
return {
    name = "Audio Reactive",
    bpm = 0,
    description = "Microphone input drives visuals",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(0) end
        dbg.create("SceneObjectNode", "mesh")
        dbg.create("PerlinFxNode", "perlin")
        dbg.create("ColorShiftNode", "color")
        dbg.create("ParticleNode", "particles")
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode", "light")
        dbg.set("perlin", "amplitude", 0.3)
        dbg.link("mesh", "entity", "perlin", "entity")
        dbg.link("mesh", "entity", "color", "entity")
    end
}
