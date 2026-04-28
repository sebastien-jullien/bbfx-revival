local ParamSpec = require "paramspec"
return {
    name = "particle_tunnel", version = 2, category = "Particle",
    description = "BBFx 2006 signature effect — ring of particles forming a traversable tunnel",
    tags = {"particle", "tunnel", "ring", "bbfx", "2006", "vj"},
    params = ParamSpec.declare({
        ParamSpec.float("emission_rate", 300, {min=50, max=1000, label="Density"}),
        ParamSpec.float("velocity", 1.0, {min=0.2, max=3, label="Speed"}),
        ParamSpec.float("particle_size", 3, {min=1, max=10, label="Size"}),
    }),
    build = function(params)
        params.template = "BBFx/ParticleTunnel"
        return {type="ParticleNode", params=params}
    end
}
