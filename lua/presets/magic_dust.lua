local ParamSpec = require "paramspec"
return {
    name = "magic_dust", version = 2, category = "Particle",
    description = "Gentle pastel magical dust floating in space",
    tags = {"particle", "magic", "dust", "fairy"},
    params = ParamSpec.declare({
        ParamSpec.float("emission_rate", 50, {min=5, max=200, label="Emission"}),
        ParamSpec.float("particle_size", 3, {min=1, max=10, label="Size"}),
    }),
    build = function(params)
        params.template = "BBFx/MagicDust"
        return {type="ParticleNode", params=params}
    end
}
