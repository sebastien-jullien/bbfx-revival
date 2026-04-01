local ParamSpec = require "paramspec"
return {
    name = "star_field", version = 2, category = "Particle",
    description = "Star field traversal",
    tags = {"stars", "space"},
    params = ParamSpec.declare({
        ParamSpec.particle("template", "BBFx/StarField", {label="Template"}),
        ParamSpec.int("star_count",2000,{min=100,max=10000,label="Stars"}),
        ParamSpec.float("speed",50,{min=10,max=500,label="Speed"}),
        ParamSpec.float("star_size",2,{min=0.5,max=10,label="Size"}),
        ParamSpec.color("star_color",{1,1,1},{label="Color"}),
        ParamSpec.bool("beat_burst",false,{label="Burst on Beat"}),
    }),
    build = function(params) return {type="ParticleNode", params=params} end
}
