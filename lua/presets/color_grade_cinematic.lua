local ParamSpec = require "paramspec"
return {
    name = "color_grade_cinematic", version = 2, category = "PostProcess",
    description = "Cinematic warm color grading with increased contrast",
    tags = {"color", "grading", "cinematic", "warm", "postprocess"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "ColorLUT"),
    }),
    build = function(params) return {type="PostProcessNode", params=params} end
}
