local ParamSpec = require "paramspec"
return {
    name = "inflate_breath", version = 2, category = "Shader",
    description = "Mesh inflation breathing effect along normals",
    tags = {"shader", "inflate", "vertex", "deformation", "geometry"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "inflate.vert"),
        ParamSpec.shader("frag_shader", "plasma.frag"),
        ParamSpec.float("amount", 0.2, {min=0, max=3, label="Amount"}),
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
