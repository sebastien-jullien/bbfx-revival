local ParamSpec = require "paramspec"
return {
    name = "oil_painting", version = 2, category = "PostProcess",
    description = "Kuwahara filter for oil painting artistic effect",
    tags = {"oil", "painting", "artistic", "kuwahara", "postprocess"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "OilPaint"),
    }),
    build = function(params) return {type="PostProcessNode", params=params} end
}
