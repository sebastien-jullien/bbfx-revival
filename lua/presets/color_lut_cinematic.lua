local ParamSpec = require "paramspec"
return {
    name = "posterize_stylized", version = 2, category = "PostProcess",
    description = "Stylized posterize effect reducing color depth",
    tags = {"posterize", "stylized", "postprocess"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "Posterize"),
    }),
    build = function(params) return {type="PostProcessNode", params=params} end
}
