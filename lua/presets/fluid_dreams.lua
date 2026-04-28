local ParamSpec = require "paramspec"
return {
    name = "fluid_dreams", version = 2, category = "Composition",
    description = "Organic fluid deformation with FBM warp shader",
    tags = {"composition", "shader", "organic", "ambient"},
    params = ParamSpec.declare({
        ParamSpec.shader("frag_shader", "fbm_warp.frag"),
    }),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="mesh", type="SceneObjectNode", params={mesh_file="geosphere8000.mesh"}},
                {name="shader", type="ShaderFxNode", params={frag_shader=params.frag_shader or "fbm_warp.frag"}},
                {name="particles", type="ParticleNode", params={particle_template="BBFx/MagicDust"}},
                {name="cam", type="CameraNode"},
                {name="light", type="LightNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="shader", toPort="entity"},
            }
        }
    end
}
