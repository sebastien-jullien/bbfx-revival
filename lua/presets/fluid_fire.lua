local ParamSpec = require "paramspec"
return {
    name = "fluid_fire", version = 2, category = "Shader",
    description = "Procedural fire flames with gradient noise and fire palette",
    tags = {"shader", "fire", "flame", "procedural", "gpu"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "fire.frag"),
        ParamSpec.float("intensity", 1.0, {min=0, max=3, label="Intensity"}),
        ParamSpec.float("speed", 1.0, {min=0, max=5, label="Speed"}),
        ParamSpec.float("detail", 3.0, {min=1, max=10, label="Detail"}),
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
