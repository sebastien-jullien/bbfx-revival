local ParamSpec = require "paramspec"
return {
    name = "mirror_kaleidoscope", version = 2, category = "PostProcess",
    description = "Kaleidoscopic mirror symmetry post-process effect",
    tags = {"kaleidoscope", "mirror", "symmetry", "postprocess"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "Kaleidoscope"),
    }),
    build = function(params) return {type="PostProcessNode", params=params} end
}
