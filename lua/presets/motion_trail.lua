local ParamSpec = require "paramspec"
return {
    name = "motion_trail", version = 2, category = "PostProcess",
    description = "Strong motion blur trails",
    tags = {"trail", "motion"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "reaction_diffusion.frag"),
        ParamSpec.float("feed_rate", 0.055, {min=0.01, max=0.1, label="Feed Rate"}),
        ParamSpec.float("kill_rate", 0.062, {min=0.01, max=0.1, label="Kill Rate"}),
    }),
    build = function(params) return {type="ShaderFxNode", params=params} end
}
