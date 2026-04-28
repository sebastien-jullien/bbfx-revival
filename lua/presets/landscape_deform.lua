local ParamSpec = require "paramspec"
return {
    name = "landscape_deform", version = 2, category = "Composition",
    description = "Shader-driven terrain deformation with color shift",
    tags = {"composition", "shader", "landscape", "deformation"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "audio_pulse.vert"),
    }),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="plane", type="SceneObjectNode", params={mesh_file="plane_1m.mesh"}},
                {name="shader", type="ShaderFxNode", params={vert_shader=params.vert_shader or "audio_pulse.vert"}},
                {name="color", type="ColorShiftNode"},
                {name="cam", type="CameraNode"},
                {name="light", type="LightNode"},
            },
            links = {
                {from="plane", fromPort="entity", to="shader", toPort="entity"},
                {from="plane", fromPort="entity", to="color", toPort="entity"},
            }
        }
    end
}
