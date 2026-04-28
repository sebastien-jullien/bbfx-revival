local ParamSpec = require "paramspec"
return {
    name = "fire", version = 2, category = "Particle",
    description = "Realistic fire with rising flames and color fading",
    tags = {"particle", "fire", "flame", "hot"},
    params = ParamSpec.declare({
        ParamSpec.float("emission_rate", 100, {min=10, max=500, label="Emission"}),
        ParamSpec.float("particle_size", 8, {min=1, max=30, label="Size"}),
    }),
    build = function(params)
        params.template = "BBFx/Fire"
        return {type="ParticleNode", params=params}
    end
}
