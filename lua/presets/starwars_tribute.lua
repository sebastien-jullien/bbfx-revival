local ParamSpec = require "paramspec"
return {
    name = "starwars_tribute", version = 2, category = "Composition",
    description = "BBFx 2006 StarWars tribute — multiple particle systems with orbital camera",
    tags = {"starwars", "tribute", "2006", "particle", "composition"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            primary = "halos",
            nodes = {
                {name="halos", type="ParticleNode", params={template="BBFx/Galaxy"}},
                {name="sparks", type="ParticleNode", params={template="BBFx/SparkBurst"}},
                {name="dust", type="ParticleNode", params={template="BBFx/StarField"}},
            },
            links = {},
            params = params,
        }
    end
}
