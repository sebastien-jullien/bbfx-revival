local ParamSpec = require "paramspec"
return {
    name = "barrel_roll", version = 2, category = "Composition",
    description = "Barrel mesh with inflate vertex deformation",
    tags = {"composition", "geometry", "mesh", "deform"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="mesh", type="SceneObjectNode", params={mesh_file="Barrel.mesh"}},
                {name="perlin", type="PerlinFxNode", params={amplitude=0.2, frequency=2.0}},
                {name="cam", type="CameraNode"},
                {name="light", type="LightNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="perlin", toPort="entity"},
            }
        }
    end
}
