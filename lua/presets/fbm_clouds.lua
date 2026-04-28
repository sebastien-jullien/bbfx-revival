local ParamSpec = require "paramspec"
return {
    name = "fbm_clouds", version = 2, category = "Shader",
    description = "FBM noise with domain warping creating fluid cloud patterns",
    tags = {"shader", "fbm", "clouds", "noise", "gpu"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "fbm_warp.frag"),
        ParamSpec.float("scale", 3.0, {min=0.5, max=10, label="Scale"}),
        ParamSpec.float("speed", 0.3, {min=0, max=2, label="Speed"}),
        ParamSpec.float("warp_strength", 1.0, {min=0, max=5, label="Warp"}),
        ParamSpec.float("octaves", 4.0, {min=1, max=8, label="Octaves"}),
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
