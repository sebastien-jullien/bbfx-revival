local ParamSpec = require "paramspec"
return {
    name = "flatten_squash", version = 2, category = "Shader",
    description = "Flatten mesh to a plane by squashing Y coordinate",
    tags = {"shader", "flatten", "vertex", "deformation", "geometry"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "flatten.vert"),
        ParamSpec.shader("frag_shader", "plasma.frag"),
        ParamSpec.float("flatness", 0.0, {min=0, max=1, label="Flatness"}),
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
