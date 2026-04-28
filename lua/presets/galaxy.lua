local ParamSpec = require "paramspec"
return {
    name = "galaxy", version = 2, category = "Particle",
    description = "Swirling galaxy of multicolored rotating particles",
    tags = {"particle", "galaxy", "space", "cosmic"},
    params = ParamSpec.declare({
        ParamSpec.float("emission_rate", 100, {min=20, max=500, label="Emission"}),
        ParamSpec.float("particle_size", 4, {min=1, max=15, label="Size"}),
    }),
    build = function(params)
        params.template = "BBFx/Galaxy"
        return {type="ParticleNode", params=params}
    end
}
