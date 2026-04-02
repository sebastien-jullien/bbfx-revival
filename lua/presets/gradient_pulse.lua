local ParamSpec = require "paramspec"
return {
    name = "gradient_pulse", version = 2, category = "Color",
    description = "Animated gradient along axis",
    tags = {"gradient", "pulse"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "plasma.frag"),
        ParamSpec.float("scale", 5.0, {min=0.1, max=20, label="Scale"}),
        ParamSpec.float("speed", 1.0, {min=0, max=10, label="Speed"}),
        ParamSpec.float("complexity", 3.0, {min=1, max=10, label="Complexity"}),
    }),
    build = function(params)
        return {
            type = "ShaderFxNode",
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
