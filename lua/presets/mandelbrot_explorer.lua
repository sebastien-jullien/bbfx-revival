local ParamSpec = require "paramspec"
return {
    name = "mandelbrot_explorer", version = 2, category = "Shader",
    description = "Interactive Mandelbrot fractal with zoom and color animation",
    tags = {"mandelbrot", "fractal", "shader", "math", "gpu"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "twist.vert"),
        ParamSpec.shader("frag_shader", "mandelbrot.frag"),
        ParamSpec.float("zoom", 1.0, {min=0.01, max=100, label="Zoom"}),
        ParamSpec.float("center_x", -0.5, {min=-2, max=2, label="Center X"}),
        ParamSpec.float("center_y", 0.0, {min=-2, max=2, label="Center Y"}),
        ParamSpec.float("max_iter", 50.0, {min=10, max=500, label="Max Iterations"}),
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
