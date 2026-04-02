local ParamSpec = require "paramspec"
return {
    name = "texture_sweep", version = 2, category = "Color",
    description = "Progressive texture transition",
    tags = {"texture", "sweep"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "sphere_trace.frag"),
        ParamSpec.float("morph", 0.5, {min=0, max=1, label="Morph"}),
        ParamSpec.float("rotation_speed", 1.0, {min=0, max=5, label="Rotation Speed"}),
    }),
    build = function(params)
        return {
            type = "ShaderFxNode",
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
