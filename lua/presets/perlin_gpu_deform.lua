local ParamSpec = require "paramspec"
return {
    name = "perlin_gpu_deform", version = 2, category = "Shader",
    description = "GPU Perlin noise displacement on mesh vertices",
    tags = {"shader", "perlin", "gpu", "deformation", "geometry"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "perlin_gpu.vert"),
        ParamSpec.shader("frag_shader", "plasma.frag"),
        ParamSpec.float("displacement", 0.2, {min=0, max=5, label="Displacement"}),
        ParamSpec.float("density", 3.0, {min=0.1, max=10, label="Density"}),
        ParamSpec.float("speed", 1.0, {min=0, max=5, label="Speed"}),
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
