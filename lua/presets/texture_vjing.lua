local ParamSpec = require "paramspec"
return {
    name = "texture_vjing", version = 2, category = "Composition",
    description = "Texture cycling on beat",
    tags = {"texture", "vj", "2006"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "twist.vert"),
        ParamSpec.shader("frag_shader", "voronoi.frag"),
        ParamSpec.float("scale", 3.0, {min=0.1, max=20, label="Scale"}),
        ParamSpec.float("speed", 0.5, {min=0, max=10, label="Speed"}),
        ParamSpec.float("edge_width", 0.05, {min=0.001, max=0.5, label="Edge Width"}),
    }),
    build = function(params) return {type="ShaderFxNode", params=params} end
}
