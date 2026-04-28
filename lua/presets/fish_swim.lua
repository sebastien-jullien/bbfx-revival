local ParamSpec = require "paramspec"
return {
    name = "fish_swim", version = 2, category = "Composition",
    description = "Fish mesh with wave vertex shader and slow orbit",
    tags = {"composition", "geometry", "mesh", "wave", "organic"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="mesh", type="SceneObjectNode", params={mesh_file="fish.mesh"}},
                {name="perlin", type="PerlinFxNode", params={amplitude=0.1, frequency=1.0, speed=0.5}},
                {name="cam", type="CameraNode"},
                {name="light", type="LightNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="perlin", toPort="entity"},
            }
        }
    end
}
