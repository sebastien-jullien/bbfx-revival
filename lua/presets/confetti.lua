local ParamSpec = require "paramspec"
return {
    name = "confetti", version = 2, category = "Particle",
    description = "Colorful confetti falling with gravity and rotation",
    tags = {"particle", "confetti", "celebration", "party"},
    params = ParamSpec.declare({
        ParamSpec.float("emission_rate", 80, {min=10, max=300, label="Emission"}),
        ParamSpec.float("particle_size", 4, {min=1, max=15, label="Size"}),
    }),
    build = function(params)
        params.template = "BBFx/Confetti"
        return {type="ParticleNode", params=params}
    end
}
