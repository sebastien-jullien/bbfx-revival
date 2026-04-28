local ParamSpec = require "paramspec"
return {
    name = "explode_mesh", version = 2, category = "Shader",
    description = "Mesh explosion with noise-driven vertex displacement along normals",
    tags = {"shader", "explode", "vertex", "deformation", "geometry"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "explode.vert"),
        ParamSpec.shader("frag_shader", "plasma.frag"),
        ParamSpec.float("intensity", 0.5, {min=0, max=5, label="Intensity"}),
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
