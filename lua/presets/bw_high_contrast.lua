local ParamSpec = require "paramspec"
return {
    name = "bw_high_contrast", version = 2, category = "PostProcess",
    description = "High contrast BW",
    tags = {"bw", "contrast"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "B&W"),
    }),
    build = function(params) return {type="CompositorNode", params=params} end
}
