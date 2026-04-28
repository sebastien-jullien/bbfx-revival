-- Template: Video Mix
-- Description: 2 video source placeholders with crossfade
-- BPM: 120
-- Nodes: 2x SceneObjectNode, ColorShiftNode, CameraNode, LightNode
return {
    name = "Video Mix",
    bpm = 120,
    description = "2 video sources with crossfade",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end
        dbg.create_with_param("SceneObjectNode", "source1", "mesh_file", "bbfx_plane.mesh")
        dbg.create_with_param("SceneObjectNode", "source2", "mesh_file", "bbfx_plane.mesh")
        dbg.create("ColorShiftNode", "color")
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode", "light")
    end
}
