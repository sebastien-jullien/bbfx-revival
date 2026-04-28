local ParamSpec = require "paramspec"
return {
    name = "moire_pattern", version = 2, category = "Shader",
    description = "Moire interference pattern from overlapping grids",
    tags = {"shader", "moire", "interference", "pattern", "gpu"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "moire.frag"),
        ParamSpec.float("frequency1", 30.0, {min=5, max=100, label="Freq 1"}),
        ParamSpec.float("frequency2", 31.0, {min=5, max=100, label="Freq 2"}),
        ParamSpec.float("rotation", 0.0, {min=0, max=360, label="Rotation"}),
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
