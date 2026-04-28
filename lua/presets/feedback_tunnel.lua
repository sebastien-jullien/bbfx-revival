local ParamSpec = require "paramspec"
return {
    name = "feedback_tunnel", version = 2, category = "PostProcess",
    description = "Feedback zoom tunnel with rotation trails",
    tags = {"feedback", "tunnel", "zoom", "trails", "temporal", "postprocess"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "FeedbackZoom"),
    }),
    build = function(params) return {type="PostProcessNode", params=params} end
}
