local ParamSpec = require "paramspec"
return {
    name = "knot_dance", version = 2, category = "Composition",
    description = "Knot mesh with spiral vertex shader and neon glow",
    tags = {"composition", "geometry", "mesh", "shader", "neon"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="mesh", type="SceneObjectNode", params={mesh_file="knot.mesh"}},
                {name="perlin", type="PerlinFxNode", params={amplitude=0.15, speed=0.8}},
                {name="color", type="ColorShiftNode", params={hue_shift=0.7, speed=0.3}},
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
