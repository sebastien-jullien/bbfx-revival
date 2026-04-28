local ParamSpec = require "paramspec"
return {
    name = "old_film", version = 2, category = "PostProcess",
    description = "Vintage film grain look",
    tags = {"film", "vintage"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "OldTV"),
    }),
    build = function(params) return {type="PostProcessNode", params=params} end
}
