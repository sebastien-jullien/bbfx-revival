local ParamSpec = require "paramspec"
return {
    name = "laser_beam", version = 2, category = "Particle",
    description = "Tight green laser beam particles shooting forward",
    tags = {"particle", "laser", "beam", "sci-fi"},
    params = ParamSpec.declare({
        ParamSpec.float("emission_rate", 80, {min=10, max=200, label="Emission"}),
        ParamSpec.float("velocity", 1.0, {min=0.5, max=3, label="Velocity"}),
    }),
    build = function(params)
        params.template = "BBFx/LaserBeam"
        return {type="ParticleNode", params=params}
    end
}
