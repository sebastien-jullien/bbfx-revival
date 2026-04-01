local ParamSpec = require "paramspec"
return {
    name = "mirror_kaleidoscope", version = 2, category = "PostProcess",
    description = "Radial symmetry",
    tags = {"kaleidoscope", "mirror"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "twist.vert"),
        ParamSpec.shader("frag_shader", "mandelbrot.frag"),
        ParamSpec.float("zoom", 1.0, {min=0.01, max=100, label="Zoom"}),
        ParamSpec.float("center_x", -0.5, {min=-2, max=2, label="Center X"}),
        ParamSpec.float("center_y", 0.0, {min=-2, max=2, label="Center Y"}),
        ParamSpec.float("max_iter", 50.0, {min=10, max=500, label="Max Iterations"}),
        ParamSpec.float("color_speed", 1.0, {min=0, max=10, label="Color Speed"}),
    }),
    build = function(params) return {type="ShaderFxNode", params=params} end
}
