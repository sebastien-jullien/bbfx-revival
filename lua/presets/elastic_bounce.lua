local ParamSpec = require "paramspec"
return {
    name = "elastic_bounce", version = 2, category = "Geometry",
    description = "Elastic squash and stretch on kick",
    tags = {"bounce", "elastic"},
    params = ParamSpec.declare({
        ParamSpec.float("displacement", 0.2, {min=0, max=5, label="Displacement"}),
        ParamSpec.float("density", 1.0, {min=0.1, max=10, label="Density"}),
        ParamSpec.float("timeDensity", 8.0, {min=0.1, max=20, label="Bounce Speed"}),
    }),
    build = function(params) return {type="PerlinFxNode", params=params} end
}
