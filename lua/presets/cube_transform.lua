local ParamSpec = require "paramspec"
return {
    name = "cube_transform", version = 2, category = "Composition",
    description = "Cube with explode vertex shader and beat triggers",
    tags = {"composition", "geometry", "mesh", "beat", "explode"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="mesh", type="SceneObjectNode", params={mesh_file="cube.mesh"}},
                {name="perlin", type="PerlinFxNode", params={amplitude=0.5, frequency=3.0}},
                {name="color", type="ColorShiftNode", params={hue_shift=0.4}},
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
