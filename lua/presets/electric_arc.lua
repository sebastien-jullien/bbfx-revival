local ParamSpec = require "paramspec"
return {
    name = "electric_arc", version = 2, category = "Particle",
    description = "Fast cyan-white electric arcs shooting upward",
    tags = {"particle", "electric", "lightning", "arc"},
    params = ParamSpec.declare({
        ParamSpec.float("emission_rate", 150, {min=20, max=500, label="Emission"}),
        ParamSpec.float("velocity", 1.0, {min=0.1, max=3, label="Velocity"}),
    }),
    build = function(params)
        params.template = "BBFx/ElectricArc"
        return {type="ParticleNode", params=params}
    end
}
