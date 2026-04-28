local ParamSpec = require "paramspec"
return {
    name = "halftone_comic", version = 2, category = "PostProcess",
    description = "CMYK-style halftone dot pattern for comic book look",
    tags = {"halftone", "comic", "dots", "print", "postprocess"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "Halftone"),
    }),
    build = function(params) return {type="PostProcessNode", params=params} end
}
