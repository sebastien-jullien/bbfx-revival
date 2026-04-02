local ParamSpec = require "paramspec"
return {
    name = "tunnel_infinite", version = 2, category = "Composition",
    description = "Infinite tunnel with pulsation",
    tags = {"tunnel", "infinite"},
    params = ParamSpec.declare({
        ParamSpec.shader("vert_shader", "passthrough.vert"),
        ParamSpec.shader("frag_shader", "tunnel.frag"),
        ParamSpec.float("speed", 1.0, {min=0, max=10, label="Speed"}),
        ParamSpec.float("twist", 2.0, {min=0, max=10, label="Twist"}),
        ParamSpec.float("radius", 3.0, {min=0.1, max=10, label="Radius"}),
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
