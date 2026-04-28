local ParamSpec = require "paramspec"
return {
    name = "audio_pulse_deform", version = 2, category = "Shader",
    description = "Mesh vertex deformation via audio_pulse shader (bass, mid, high bands)",
    tags = {"shader", "vertex", "deformation", "geometry", "pulse"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "audio_pulse.vert"),
        ParamSpec.shader("frag_shader", "plasma.frag"),
        ParamSpec.float("bass", 0.0, {min=0, max=2, label="Bass"}),
        ParamSpec.float("mid", 0.0, {min=0, max=2, label="Mid"}),
        ParamSpec.float("high", 0.0, {min=0, max=2, label="High"}),
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
