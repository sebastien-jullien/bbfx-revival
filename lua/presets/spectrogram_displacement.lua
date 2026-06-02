-- Preset: spectrogram_displacement
-- Sprint S4 KILLER FEATURE demo: SpectrogramTextureNode + ShaderFxNode (displacement vertex).
-- The audio waterfall texture drives a Geosphere displacement in real time.
local ParamSpec = require "paramspec"
return {
    name = "spectrogram_displacement", version = 2, category = "VJ",  -- v2 = Sprint S8 Lot AE : material flow wired
    description = "Audio spectrogram displaces a Geosphere in real time (S4 killer)",
    tags = {"vj", "audio", "spectrogram", "displacement", "killer", "demo"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            primary = "mesh",
            nodes = {
                {name="cam",     type="CameraNode"},
                {name="light",   type="LightNode"},
                {name="mesh",    type="SceneObjectNode"},
                {name="audio",   type="AudioCaptureNode"},
                {name="spectro", type="SpectrogramTextureNode",
                  params={colormap="viridis", frequency_scale="log"}},
                {name="color",   type="ColorShiftNode"},
                -- v3.5.2 Sprint S8 Lot AE : MaterialBridge applique la texture
                -- spectrogramme (texture_out → auto-wrap → material) sur la
                -- Geosphere via Pattern 3 + extension multi-target inutile ici
                -- (target = SceneObjectNode classique).
                {name="bridge",  type="MaterialBridgeNode",
                  params={lighting_mode="lit"}},
            },
            links = {
                {from="audio", fromPort="frame_ready", to="spectro", toPort="audio"},
                {from="mesh",  fromPort="entity",      to="color",   toPort="entity"},
                -- Material flow : spectrogram texture_out → bridge auto-wrap → mesh
                {from="spectro", fromPort="texture_ready", to="bridge", toPort="material_source"},
                {from="mesh",    fromPort="entity",        to="bridge", toPort="entity"},
            },
            params = params,
        }
    end
}
