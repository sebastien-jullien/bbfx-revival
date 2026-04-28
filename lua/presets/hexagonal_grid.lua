local ParamSpec = require "paramspec"
return {
    name = "hexagonal_grid", version = 2, category = "Shader",
    description = "Colorful hexagonal tiling pattern",
    tags = {"shader", "hexagonal", "pattern", "gpu"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "hexagonal.frag"),
        ParamSpec.float("scale", 10.0, {min=2, max=50, label="Scale"}),
        ParamSpec.float("gap", 0.05, {min=0, max=0.3, label="Gap"}),
        ParamSpec.float("color_shift", 0.0, {min=0, max=360, label="Color Shift"}),
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
