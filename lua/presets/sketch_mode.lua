local ParamSpec = require "paramspec"
return {
    name = "sketch_mode", version = 2, category = "PostProcess",
    description = "Pencil cross-hatch drawing effect",
    tags = {"sketch", "pencil", "hatch", "drawing", "artistic", "postprocess"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "CrossHatch"),
    }),
    build = function(params) return {type="PostProcessNode", params=params} end
}
