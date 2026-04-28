local ParamSpec = require "paramspec"
return {
    name = "fractal_explorer", version = 2, category = "Composition",
    description = "Fullscreen Mandelbrot fractal with auto-zoom",
    tags = {"composition", "shader", "fractal", "math"},
    params = ParamSpec.declare({
        ParamSpec.shader("frag_shader", "mandelbrot.frag"),
    }),
    build = function(params)
        return {
            type = "CompositionNode",
            nodes = {
                {name="plane", type="SceneObjectNode", params={mesh_file="bbfx_plane.mesh"}},
                {name="shader", type="ShaderFxNode", params={frag_shader=params.frag_shader or "mandelbrot.frag"}},
                {name="cam", type="CameraNode"},
            },
            links = {
                {from="plane", fromPort="entity", to="shader", toPort="entity"},
            }
        }
    end
}
