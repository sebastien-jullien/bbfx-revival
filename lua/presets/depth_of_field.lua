local ParamSpec = require "paramspec"
return {
    name = "depth_of_field", version = 2, category = "PostProcess",
    description = "Cinematic DOF blur",
    tags = {"dof", "cinematic"},
    params = ParamSpec.declare({
        ParamSpec.compositor("compositor", "DOF"),
    }),
    build = function(params) return {type="CompositorNode", params=params} end
}
