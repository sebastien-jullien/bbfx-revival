local ParamSpec = require "paramspec"
return {
    name = "bonneballe_classic", version = 2, category = "Composition",
    description = "Flagship: Geosphere + Perlin + ColorShift + Bloom + orbit camera",
    tags = {"composition", "flagship", "geometry", "postprocess", "beat"},
    params = ParamSpec.declare({
        ParamSpec.float("amplitude", 0.3, {min=0, max=2}),
        ParamSpec.float("frequency", 2.0, {min=0.1, max=10}),
        ParamSpec.float("hue_shift", 0.5, {min=0, max=1}),
    }),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="mesh", type="SceneObjectNode"},
                {name="perlin", type="PerlinFxNode", params={amplitude=params.amplitude or 0.3, frequency=params.frequency or 2.0}},
                {name="color", type="ColorShiftNode", params={hue_shift=params.hue_shift or 0.5}},
                {name="bloom", type="PostProcessNode", params={compositor="Bloom"}},
                {name="cam", type="CameraNode"},
                {name="light", type="LightNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="perlin", toPort="entity"},
                {from="mesh", fromPort="entity", to="color", toPort="entity"},
            }
        }
    end
}
