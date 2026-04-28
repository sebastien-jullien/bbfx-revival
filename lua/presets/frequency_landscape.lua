local ParamSpec = require "paramspec"
return {
    name = "frequency_landscape", version = 2, category = "Composition",
    description = "Audio-reactive landscape: plane deformed by frequency bands",
    tags = {"composition", "audio", "shader", "landscape"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "audio_pulse.vert"),
    }),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="plane", type="SceneObjectNode", params={mesh_file="plane_1m.mesh"}},
                {name="shader", type="ShaderFxNode", params={vert_shader=params.vert_shader or "audio_pulse.vert"}},
                {name="cam", type="CameraNode"},
                {name="light", type="LightNode"},
            },
            links = {
                {from="plane", fromPort="entity", to="shader", toPort="entity"},
            }
        }
    end
}
