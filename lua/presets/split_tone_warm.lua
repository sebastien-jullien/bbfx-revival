local ParamSpec = require "paramspec"
return {
    name = "vignette_warm", version = 2, category = "PostProcess",
    description = "Warm vignette darkening at screen edges",
    tags = {"vignette", "warm", "postprocess"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "Vignette"),
    }),
    build = function(params) return {type="PostProcessNode", params=params} end
}
