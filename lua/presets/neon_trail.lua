local ParamSpec = require "paramspec"
return {
    name = "neon_trail", version = 2, category = "Particle",
    description = "Bright neon green oriented trails shooting forward",
    tags = {"particle", "neon", "trail", "glow"},
    params = ParamSpec.declare({
        ParamSpec.float("emission_rate", 100, {min=10, max=300, label="Emission"}),
        ParamSpec.float("velocity", 1.0, {min=0.2, max=3, label="Velocity"}),
    }),
    build = function(params)
        params.template = "BBFx/NeonTrail"
        return {type="ParticleNode", params=params}
    end
}
