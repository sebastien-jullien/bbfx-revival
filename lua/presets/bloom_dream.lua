local ParamSpec = require "paramspec"
return {
    name = "bloom_dream", version = 2, category = "PostProcess",
    description = "Overexposed bloom for dreamy ethereal look",
    tags = {"bloom", "dream", "glow", "ethereal"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "Bloom"),
    }),
    build = function(params) return {type="CompositorNode", params=params} end
}
