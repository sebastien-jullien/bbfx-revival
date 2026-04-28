local ParamSpec = require "paramspec"
return {
    name = "waveform_display", version = 2, category = "Shader",
    description = "Audio waveform visualization with glow",
    tags = {"shader", "waveform", "audio", "visualization", "gpu"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "waveform.frag"),
        ParamSpec.float("amplitude", 0.3, {min=0, max=1, label="Amplitude"}),
        ParamSpec.float("frequency", 4.0, {min=0.5, max=20, label="Frequency"}),
        ParamSpec.float("thickness", 0.02, {min=0.001, max=0.1, label="Thickness"}),
    }),
    build = function(params)
        return {
            type = "CompositionNode",
            primary = "fx",
            nodes = {
                {name="mesh", type="SceneObjectNode"},
                {name="fx",   type="ShaderFxNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="fx", toPort="entity"},
            },
            params = params,
        }
    end
}
