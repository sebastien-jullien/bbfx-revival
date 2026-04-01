local ParamSpec = require "paramspec"
return {
    name = "fireflies", version = 2, category = "Particle",
    description = "Luminous firefly swarm",
    tags = {"fireflies", "glow"},
    params = ParamSpec.declare({
        ParamSpec.particle("template", "Examples/GreenyNimbus", {label="Template"}),
        ParamSpec.int("count",500,{min=50,max=3000,label="Count"}),
        ParamSpec.float("area_size",100,{min=20,max=500,label="Area"}),
        ParamSpec.float("randomness",30,{min=5,max=100,label="Randomness"}),
        ParamSpec.color("glow_color",{0.5,1.0,0.3},{label="Glow Color"}),
    }),
    build = function(params) return {type="ParticleNode", params=params} end
}
