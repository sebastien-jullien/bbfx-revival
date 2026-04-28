local ParamSpec = require "paramspec"
return {
    name = "perlin_glitch", version = 2, category = "Geometry",
    description = "Nervous high-frequency Perlin noise with fast animation",
    tags = {"perlin", "glitch", "noise", "geometry", "deformation"},
    params = ParamSpec.declare({
        ParamSpec.mesh("mesh", "geosphere4500.mesh", {label="Mesh"}),
        ParamSpec.float("displacement", 0.3, {min=0, max=20, label="Displacement"}),
        ParamSpec.float("density", 6.0, {min=0.1, max=20, label="Noise Scale"}),
        ParamSpec.float("timeDensity", 8.0, {min=0.1, max=20, label="Speed"}),
    }),
    build = function(params)
        return {
            type = "PerlinFxNode",
            primary = "fx",
            nodes = {
                {name="mesh", type="SceneObjectNode", params={mesh_file=params.mesh or "geosphere4500.mesh"}},
                {name="fx",   type="PerlinFxNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="fx", toPort="entity"},
            },
            params = params,
        }
    end
}
