local ParamSpec = require "paramspec"
return {
    name = "julia_explorer", version = 2, category = "Shader",
    description = "Interactive Julia set fractal with animable c parameters",
    tags = {"shader", "julia", "fractal", "math", "gpu"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "julia.frag"),
        ParamSpec.float("zoom", 1.0, {min=0.1, max=50, label="Zoom"}),
        ParamSpec.float("cx", -0.7, {min=-2, max=2, label="C.x"}),
        ParamSpec.float("cy", 0.27, {min=-2, max=2, label="C.y"}),
        ParamSpec.float("max_iter", 100, {min=10, max=500, label="Iterations"}),
        ParamSpec.float("color_speed", 1.0, {min=0, max=10, label="Color Speed"}),
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
