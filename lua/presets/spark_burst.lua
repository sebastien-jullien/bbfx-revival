local ParamSpec = require "paramspec"
return {
    name = "spark_burst", version = 2, category = "Particle",
    description = "Spark explosion on beat",
    tags = {"sparks", "beat"},
    params = ParamSpec.declare({
        ParamSpec.particle("template", "BBFx/SparkBurst", {label="Template"}),
        ParamSpec.int("sparks",200,{min=50,max=1000,label="Sparks"}),
        ParamSpec.float("velocity",300,{min=50,max=800,label="Velocity"}),
        ParamSpec.color("start_color",{1,0.8,0.2},{label="Start"}),
        ParamSpec.color("end_color",{1,0.1,0},{label="End"}),
        ParamSpec.bool("beat_trigger",true,{label="Beat Trigger"}),
    }),
    build = function(params) return {type="ParticleNode", params=params} end
}
