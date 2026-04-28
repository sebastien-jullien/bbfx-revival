-- Template: Shader Lab
-- Description: Fullscreen plane with GPU shader
-- BPM: 120
-- Nodes: SceneObjectNode, ShaderFxNode, CameraNode
return {
    name = "Shader Lab",
    bpm = 120,
    description = "Explore GPU shaders",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end
        dbg.create_with_param("SceneObjectNode", "mesh", "mesh_file", "bbfx_plane.mesh")
        dbg.create("ShaderFxNode", "shader")
        dbg.create("CameraNode", "cam")
        dbg.link("mesh", "entity", "shader", "entity")
    end
}
