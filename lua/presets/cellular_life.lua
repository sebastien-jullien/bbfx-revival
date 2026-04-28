local ParamSpec = require "paramspec"
return {
    name = "cellular_life", version = 2, category = "Shader",
    description = "Procedural Game of Life approximation",
    tags = {"shader", "cellular", "life", "automata", "gpu"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "cellular_automata.frag"),
        ParamSpec.float("scale", 20.0, {min=5, max=100, label="Scale"}),
        ParamSpec.float("speed", 1.0, {min=0, max=5, label="Speed"}),
        ParamSpec.float("threshold", 0.5, {min=0, max=1, label="Threshold"}),
    }),
    build = function(params)
        return {
            type = "CompositionNode",
            primary = "fx",
            nodes = {
                {name="mesh", type="SceneObjectNode"},
                {name="fx",   type="ShaderFxNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="fx", toPort="entity"},
            },
            params = params,
        }
    end
}
