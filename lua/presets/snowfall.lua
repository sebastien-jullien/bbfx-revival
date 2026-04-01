local ParamSpec = require "paramspec"
return {
    name = "snowfall", version = 2, category = "Particle",
    description = "Gentle snowfall with wind",
    tags = {"snow", "winter"},
    params = ParamSpec.declare({
        ParamSpec.particle("template", "BBFx/Snowfall", {label="Template"}),
        ParamSpec.int("flake_count",2000,{min=200,max=5000,label="Flakes"}),
        ParamSpec.float("fall_speed",20,{min=5,max=100,label="Speed"}),
        ParamSpec.float("wind_strength",10,{min=0,max=50,label="Wind"}),
        ParamSpec.float("flake_size",4,{min=1,max=15,label="Size"}),
    }),
    build = function(params) return {type="ParticleNode", params=params} end
}
