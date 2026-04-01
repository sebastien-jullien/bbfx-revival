local ParamSpec = require "paramspec"
return {
    name = "starwars_tribute", version = 2, category = "Composition",
    description = "2006 StarWars tribute",
    tags = {"starwars", "tribute", "2006"},
    params = ParamSpec.declare({
        ParamSpec.particle("template", "Examples/Aureola", {label="Template"}),
        ParamSpec.float("bpm",144,{min=60,max=200,label="BPM"}),
        ParamSpec.float("intensity",1.0,{min=0.5,max=3,label="Intensity"}),
        ParamSpec.int("particle_systems",6,{min=2,max=10,label="Systems"}),
        ParamSpec.bool("camera_orbit",true,{label="Camera Orbit"}),
    }),
    build = function(params) return {type="ParticleNode", params=params} end
}
