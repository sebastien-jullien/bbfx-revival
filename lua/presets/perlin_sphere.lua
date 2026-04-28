local ParamSpec = require "paramspec"
return {
    name = "perlin_sphere", version = 2, category = "Composition",
    description = "Iconic BonneBalle with Perlin displacement on geosphere",
    tags = {"perlin", "sphere", "iconic", "composition"},
    params = ParamSpec.declare({
        ParamSpec.mesh("mesh", "geosphere4500.mesh", {label="Mesh"}),
        ParamSpec.float("displacement", 0.15, {min=0, max=20, label="Displacement"}),
        ParamSpec.float("density", 4.0, {min=0.1, max=10, label="Noise Scale"}),
        ParamSpec.float("timeDensity", 5.0, {min=0.1, max=20, label="Time Density"}),
    }),
    build = function(params)
        return {
            type = "CompositionNode",
            primary = "fx",
            nodes = {
                {name="mesh", type="SceneObjectNode", params={mesh_file="geosphere4500.mesh"}},
                {name="fx",   type="PerlinFxNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="fx", toPort="entity"},
            },
            params = params,
        }
    end
}
