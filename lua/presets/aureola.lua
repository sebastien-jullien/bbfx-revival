local ParamSpec = require "paramspec"
return {
    name = "aureola", version = 2, category = "Particle",
    description = "Luminous halo",
    tags = {"halo", "aureola"},
    params = ParamSpec.declare({
        ParamSpec.particle("template", "Examples/Aureola", {label="Template"}),
        ParamSpec.float("emission_rate",5,{min=1,max=20,label="Rate"}),
        ParamSpec.float("size",200,{min=50,max=500,label="Size"}),
        ParamSpec.float("rotation_speed",30,{min=0,max=180,label="Rotation"}),
        ParamSpec.color("color",{1,0.9,0.5},{label="Color"}),
        ParamSpec.float("alpha",0.5,{min=0.1,max=1,label="Alpha"}),
    }),
    build = function(params) return {type="ParticleNode", params=params} end
}
