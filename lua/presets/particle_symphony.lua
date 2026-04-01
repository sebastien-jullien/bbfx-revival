local ParamSpec = require "paramspec"
return {
    name = "particle_symphony", version = 2, category = "Composition",
    description = "4 particles driven by freq bands",
    tags = {"particles", "audio"},
    params = ParamSpec.declare({
        ParamSpec.particle("template", "Examples/PurpleFountain", {label="Template"}),
        ParamSpec.float("sensitivity",1.0,{min=0.1,max=5,label="Sensitivity"}),
        ParamSpec.color("bass_color",{1,0.2,0.1},{label="Bass"}),
        ParamSpec.color("mid_color",{0.2,1,0.3},{label="Mid"}),
        ParamSpec.color("high_color",{0.3,0.5,1},{label="High"}),
    }),
    build = function(params) return {type="ParticleNode", params=params} end
}
