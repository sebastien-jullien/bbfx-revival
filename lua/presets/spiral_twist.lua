local ParamSpec = require "paramspec"
return {
    name = "spiral_twist", version = 2, category = "Shader",
    description = "Helical twist deformation around Y axis",
    tags = {"shader", "spiral", "twist", "vertex", "deformation"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "spiral.vert"),
        ParamSpec.shader("frag_shader", "plasma.frag"),
        ParamSpec.float("twist", 1.0, {min=0, max=10, label="Twist"}),
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
