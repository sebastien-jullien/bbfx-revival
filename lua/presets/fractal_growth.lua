local ParamSpec = require "paramspec"
return {
    name = "fractal_growth", version = 2, category = "Geometry",
    description = "Fractal growth pattern",
    tags = {"fractal", "growth"},
    params = ParamSpec.declare({
        ParamSpec.mesh("mesh", "geosphere8000.mesh", {label="Mesh"}),
        ParamSpec.float("displacement", 0.4, {min=0, max=10, label="Displacement"}),
        ParamSpec.float("density", 6.0, {min=0.1, max=20, label="Density"}),
        ParamSpec.float("timeDensity", 2.0, {min=0.1, max=10, label="Time Density"}),
    }),
    build = function(params)
        return {
            type = "PerlinFxNode",
            primary = "fx",
            nodes = {
                {name="mesh", type="SceneObjectNode", params={mesh_file="ogrehead.mesh"}},
                {name="fx",   type="PerlinFxNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="fx", toPort="entity"},
            },
            params = params,
        }
    end
}
