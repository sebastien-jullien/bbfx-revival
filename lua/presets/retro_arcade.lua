local ParamSpec = require "paramspec"
return {
    name = "retro_arcade", version = 2, category = "Composition",
    description = "Retro pixelated look with ASCII and posterize effects",
    tags = {"composition", "retro", "postprocess", "pixel"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="mesh", type="SceneObjectNode"},
                {name="color", type="ColorShiftNode", params={saturation=0.8}},
                {name="pp_ascii", type="PostProcessNode", params={compositor="ASCII"}},
                {name="pp_pixel", type="PostProcessNode", params={compositor="Pixelate"}},
                {name="cam", type="CameraNode"},
                {name="light", type="LightNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="color", toPort="entity"},
            }
        }
    end
}
