local ParamSpec = require "paramspec"
return {
    name = "geosphere_explode", version = 2, category = "Geometry",
    description = "Centrifugal explosion on beat",
    tags = {"explosion", "beat"},
    params = ParamSpec.declare({
        ParamSpec.mesh("mesh", "geosphere4500.mesh", {label="Mesh"}),
        ParamSpec.float("displacement", 0.5, {min=0, max=50, label="Displacement"}),
        ParamSpec.float("density", 1.0, {min=0.1, max=10, label="Density"}),
        ParamSpec.float("timeDensity", 10.0, {min=0.1, max=20, label="Time Density"}),
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
