local ParamSpec = require "paramspec"
return {
    name = "motion_trail", version = 2, category = "Shader",
    description = "Temporal echo trails with directional blur and decay",
    tags = {"trail", "motion", "echo", "shader", "gpu"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "motion_trail.frag"),
        ParamSpec.float("trail_length", 0.3, {min=0, max=1, label="Trail Length"}),
        ParamSpec.float("decay", 0.85, {min=0.5, max=0.99, label="Decay"}),
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
