local ParamSpec = require "paramspec"
return {
    name = "heat_distort", version = 2, category = "PostProcess",
    description = "Thermal distortion",
    tags = {"heat", "distortion"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "flowfield.frag"),
        ParamSpec.float("scale", 3.0, {min=0.1, max=20, label="Scale"}),
        ParamSpec.float("speed", 1.0, {min=0, max=10, label="Speed"}),
        ParamSpec.float("curl", 2.0, {min=0, max=10, label="Curl"}),
    }),
    build = function(params) return {type="ShaderFxNode", params=params} end
}
