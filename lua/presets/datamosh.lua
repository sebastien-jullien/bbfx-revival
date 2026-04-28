local ParamSpec = require "paramspec"
return {
    name = "datamosh", version = 2, category = "Shader",
    description = "Datamosh glitch effect with pixel displacement",
    tags = {"shader", "datamosh", "glitch", "gpu"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "datamosh.frag"),
        ParamSpec.float("intensity", 0.5, {min=0, max=1, label="Intensity"}),
        ParamSpec.float("block_size", 16.0, {min=2, max=64, label="Block Size"}),
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
