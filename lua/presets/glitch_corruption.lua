local ParamSpec = require "paramspec"
return {
    name = "glitch_corruption", version = 2, category = "Composition",
    description = "Stacked digital corruption: VHS distortion over feedback zoom trails",
    tags = {"glitch", "corruption", "feedback", "digital", "postprocess", "composition"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="mesh", type="SceneObjectNode"},
                {name="pp_vhs", type="PostProcessNode", params={compositor="VHS"}},
                {name="pp_feedback", type="PostProcessNode", params={compositor="FeedbackZoom"}},
                {name="cam", type="CameraNode"},
                {name="light", type="LightNode"},
            },
            links = {}
        }
    end
}
