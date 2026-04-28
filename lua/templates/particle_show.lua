-- Template: Particle Show
-- Description: 4 particle systems with different templates
-- BPM: 140
-- Nodes: 4x ParticleNode, CameraNode, LightNode
return {
    name = "Particle Show",
    bpm = 140,
    description = "4 particle systems with triggers",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(140) end
        dbg.create("ParticleNode", "fire")
        dbg.create("ParticleNode", "sparks")
        dbg.create("ParticleNode", "snow")
        dbg.create("ParticleNode", "stars")
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode", "light")
    end
}
