local ParamSpec = require "paramspec"
return {
    name = "rain_drops", version = 2, category = "Particle",
    description = "Gentle rain",
    tags = {"rain", "weather"},
    params = ParamSpec.declare({
        ParamSpec.particle("template", "Examples/Rain", {label="Template"}),
        ParamSpec.float("intensity",100,{min=10,max=500,label="Drops/sec"}),
        ParamSpec.float("speed",50,{min=10,max=200,label="Speed"}),
        ParamSpec.color("drop_color",{0.7,0.8,1.0},{label="Color"}),
        ParamSpec.float("wind",0,{min=-50,max=50,label="Wind"}),
    }),
    build = function(params) return {type="ParticleNode", params=params} end
}
