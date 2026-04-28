local ParamSpec = require "paramspec"
return {
    name = "wave_gpu_morph", version = 2, category = "Shader",
    description = "GPU wave deformation on mesh surface",
    tags = {"shader", "wave", "gpu", "deformation", "geometry"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "wave_gpu.vert"),
        ParamSpec.shader("frag_shader", "plasma.frag"),
        ParamSpec.float("amplitude", 0.3, {min=0, max=2, label="Amplitude"}),
        ParamSpec.float("frequency", 3.0, {min=0.1, max=20, label="Frequency"}),
        ParamSpec.float("speed", 2.0, {min=0, max=10, label="Speed"}),
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
