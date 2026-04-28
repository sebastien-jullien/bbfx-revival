local ParamSpec = require "paramspec"
return {
    name = "radial_zoom", version = 2, category = "PostProcess",
    description = "Radial zoom blur from center point",
    tags = {"radial", "blur", "zoom", "speed", "postprocess"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "RadialBlur"),
    }),
    build = function(params) return {type="PostProcessNode", params=params} end
}
