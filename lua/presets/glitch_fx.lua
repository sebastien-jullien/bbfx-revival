local ParamSpec = require "paramspec"
return {
    name = "glitch_fx", version = 2, category = "PostProcess",
    description = "Digital corruption: RGB offset + scanlines + noise blocks",
    tags = {"glitch", "digital", "corruption", "chromatic"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "truchet.frag"),
        ParamSpec.float("scale", 5.0, {min=0.1, max=20, label="Scale"}),
        ParamSpec.float("line_width", 0.1, {min=0.01, max=0.5, label="Line Width"}),
    }),
    build = function(params) return {type="ShaderFxNode", params=params} end
}
