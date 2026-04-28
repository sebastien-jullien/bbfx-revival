local ParamSpec = require "paramspec"
return {
    name = "pixel_sort", version = 2, category = "Shader",
    description = "Pixel sorting glitch effect",
    tags = {"shader", "pixel_sort", "glitch", "gpu"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "pixel_sort.frag"),
        ParamSpec.float("threshold", 0.5, {min=0, max=1, label="Threshold"}),
        ParamSpec.float("direction", 0.0, {min=0, max=1, label="Direction"}),
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
