local ParamSpec = require "paramspec"
return {
    name = "smoke_rise", version = 2, category = "Particle",
    description = "Rising atmospheric smoke",
    tags = {"smoke", "atmosphere"},
    params = ParamSpec.declare({
        ParamSpec.particle("template", "Examples/Smoke", {label="Template"}),
        ParamSpec.float("emission_rate",15,{min=1,max=100,label="Rate"}),
        ParamSpec.float("rise_speed",150,{min=20,max=400,label="Speed"}),
        ParamSpec.float("particle_size",55,{min=10,max=200,label="Size"}),
        ParamSpec.float("lifetime",4,{min=1,max=15,label="Lifetime"}),
    }),
    build = function(params) return {type="ParticleNode", params=params} end
}
