local ParamSpec = require "paramspec"
return {
    name = "glitch_fx", version = 2, category = "Shader",
    description = "Digital corruption: RGB offset, block displacement, scanlines, noise bursts",
    tags = {"glitch", "digital", "corruption", "chromatic", "shader", "gpu"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "glitch_block.frag"),
        ParamSpec.float("intensity", 0.5, {min=0, max=1, label="Intensity"}),
        ParamSpec.float("block_size", 8.0, {min=1, max=64, label="Block Size"}),
        ParamSpec.float("rgb_shift", 0.01, {min=0, max=0.05, label="RGB Shift"}),
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
