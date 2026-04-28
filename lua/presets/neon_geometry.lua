local ParamSpec = require "paramspec"
return {
    name = "neon_geometry", version = 2, category = "Composition",
    description = "Multi-mesh neon wireframe geometry with trails",
    tags = {"composition", "geometry", "neon", "postprocess"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="mesh1", type="SceneObjectNode", params={mesh_file="torus.mesh"}},
                {name="mesh2", type="SceneObjectNode", params={mesh_file="torusknot.mesh"}},
                {name="perlin", type="PerlinFxNode", params={amplitude=0.15, speed=0.5}},
                {name="color", type="ColorShiftNode", params={hue_shift=0.8, speed=0.2}},
                {name="cam", type="CameraNode"},
                {name="light", type="LightNode"},
            },
            links = {
                {from="mesh1", fromPort="entity", to="perlin", toPort="entity"},
                {from="mesh1", fromPort="entity", to="color", toPort="entity"},
            }
        }
    end
}
