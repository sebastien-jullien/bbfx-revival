local ParamSpec = require "paramspec"
return {
    name = "jet_exhaust", version = 2, category = "Particle",
    description = "Jet engine flame",
    tags = {"jet", "flame"},
    params = ParamSpec.declare({
        ParamSpec.particle("template", "Examples/JetEngine1", {label="Template"}),
        ParamSpec.float("emission_rate",100,{min=20,max=300,label="Rate"}),
        ParamSpec.float("velocity",300,{min=50,max=600,label="Velocity"}),
        ParamSpec.color("core_color",{1,1,0.5},{label="Core"}),
        ParamSpec.color("outer_color",{1,0.3,0},{label="Outer"}),
        ParamSpec.enum("temperature","warm",{"warm","cold","plasma"},{label="Temperature"}),
    }),
    build = function(params) return {type="ParticleNode", params=params} end
}
