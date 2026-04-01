local ParamSpec = require "paramspec"
return {
    name = "mesh_morph_cycle", version = 2, category = "Geometry",
    description = "Cycle between deformations",
    tags = {"morph", "cycle"},
    params = ParamSpec.declare({
        ParamSpec.mesh("mesh", "geosphere4500.mesh", {label="Mesh"}),
        ParamSpec.float("displacement", 0.2, {min=0, max=15, label="Displacement"}),
        ParamSpec.float("density", 2.0, {min=0.1, max=10, label="Density"}),
        ParamSpec.float("timeDensity", 0.5, {min=0.1, max=10, label="Time Density"}),
    }),
    build = function(params) return {type="PerlinFxNode", params=params} end
}
