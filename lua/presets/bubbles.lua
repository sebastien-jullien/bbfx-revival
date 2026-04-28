local ParamSpec = require "paramspec"
return {
    name = "bubbles", version = 2, category = "Particle",
    description = "Gentle translucent bubbles rising and growing",
    tags = {"particle", "bubbles", "water", "underwater"},
    params = ParamSpec.declare({
        ParamSpec.float("emission_rate", 30, {min=5, max=100, label="Emission"}),
        ParamSpec.float("particle_size", 5, {min=1, max=20, label="Size"}),
    }),
    build = function(params)
        params.template = "BBFx/Bubbles"
        return {type="ParticleNode", params=params}
    end
}
