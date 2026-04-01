local ParamSpec = require "paramspec"
return {
    name = "vertex_noise", version = 2, category = "Geometry",
    description = "High frequency vertex noise",
    tags = {"noise", "glitch"},
    params = ParamSpec.declare({
        ParamSpec.mesh("mesh", "geosphere4500.mesh", {label="Mesh"}),
        ParamSpec.float("displacement", 0.3, {min=0, max=20, label="Displacement"}),
        ParamSpec.float("density", 6.0, {min=0.1, max=20, label="Density"}),
        ParamSpec.float("timeDensity", 8.0, {min=0.1, max=20, label="Noise Speed"}),
    }),
    build = function(params) return {type="PerlinFxNode", params=params} end
}
