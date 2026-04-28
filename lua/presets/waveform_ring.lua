local ParamSpec = require "paramspec"
return {
    name = "waveform_ring", version = 2, category = "Composition",
    description = "Torus deformed by audio waveform",
    tags = {"composition", "audio", "shader", "geometry"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "audio_pulse.vert"),
    }),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="torus", type="SceneObjectNode", params={mesh_file="torus.mesh"}},
                {name="shader", type="ShaderFxNode", params={vert_shader=params.vert_shader or "audio_pulse.vert"}},
                {name="color", type="ColorShiftNode", params={hue_shift=0.3, speed=0.1}},
                {name="cam", type="CameraNode"},
                {name="light", type="LightNode"},
            },
            links = {
                {from="torus", fromPort="entity", to="shader", toPort="entity"},
                {from="torus", fromPort="entity", to="color", toPort="entity"},
            }
        }
    end
}
