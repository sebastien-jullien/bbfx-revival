local ParamSpec = require "paramspec"
return {
    name = "tunnel_party", version = 2, category = "Composition",
    description = "Particle tunnel with beat flash and fog",
    tags = {"composition", "particle", "tunnel", "beat"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="tunnel", type="ParticleNode", params={template="BBFx/ParticleTunnel"}},
                {name="light", type="LightNode"},
                {name="cam", type="CameraNode"},
            },
            links = {}
        }
    end
}
