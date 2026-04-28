local ParamSpec = require "paramspec"
return {
    name = "perlin_organic", version = 2, category = "Geometry",
    description = "Smooth organic Perlin breathing on high-detail geosphere",
    tags = {"perlin", "organic", "slow", "geometry", "deformation"},
    params = ParamSpec.declare({
        ParamSpec.mesh("mesh", "geosphere8000.mesh", {label="Mesh"}),
        ParamSpec.float("displacement", 0.2, {min=0, max=5, label="Displacement"}),
        ParamSpec.float("density", 3.0, {min=0.1, max=10, label="Noise Scale"}),
        ParamSpec.float("timeDensity", 2.0, {min=0.1, max=10, label="Speed"}),
    }),
    build = function(params)
        return {
            type = "PerlinFxNode",
            primary = "fx",
            nodes = {
                {name="mesh", type="SceneObjectNode", params={mesh_file=params.mesh or "geosphere8000.mesh"}},
                {name="fx",   type="PerlinFxNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="fx", toPort="entity"},
            },
            params = params,
        }
    end
}
