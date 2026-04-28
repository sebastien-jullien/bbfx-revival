local ParamSpec = require "paramspec"
return {
    name = "matrix_rain", version = 2, category = "Particle",
    description = "Green digital rain falling from above like The Matrix",
    tags = {"particle", "matrix", "rain", "digital", "code"},
    params = ParamSpec.declare({
        ParamSpec.float("emission_rate", 200, {min=50, max=1000, label="Emission"}),
        ParamSpec.float("particle_size", 4, {min=1, max=10, label="Size"}),
    }),
    build = function(params)
        params.template = "BBFx/MatrixRain"
        return {type="ParticleNode", params=params}
    end
}
