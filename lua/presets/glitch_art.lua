local ParamSpec = require "paramspec"
return {
    name = "glitch_art", version = 2, category = "Composition",
    description = "Stacked glitch post-process effects",
    tags = {"composition", "glitch", "postprocess", "digital"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="mesh", type="SceneObjectNode"},
                {name="perlin", type="PerlinFxNode", params={amplitude=0.4, frequency=3.0}},
                {name="pp_glitch", type="PostProcessNode", params={compositor="VHS"}},
                {name="pp_chromatic", type="PostProcessNode", params={compositor="ChromaticAberration"}},
                {name="cam", type="CameraNode"},
                {name="light", type="LightNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="perlin", toPort="entity"},
            }
        }
    end
}
