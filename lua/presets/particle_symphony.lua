local ParamSpec = require "paramspec"
return {
    name = "particle_symphony", version = 2, category = "Composition",
    description = "4 particle systems: fire, electric arc, sparks, magic dust",
    tags = {"particle", "composition", "symphony", "multi"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            primary = "bass_particles",
            nodes = {
                {name="bass_particles", type="ParticleNode", params={template="BBFx/Fire"}},
                {name="mid_particles", type="ParticleNode", params={template="BBFx/ElectricArc"}},
                {name="high_particles", type="ParticleNode", params={template="BBFx/SparkBurst"}},
                {name="treble_particles", type="ParticleNode", params={template="BBFx/MagicDust"}},
            },
            links = {},
            params = params,
        }
    end
}
