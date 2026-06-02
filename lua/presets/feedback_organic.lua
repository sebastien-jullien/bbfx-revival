-- Preset: feedback_organic
-- Sprint S4 demo: TextureFeedback + NoiseTexture + organic Geosphere
-- Demonstrates the Sprint S3 feedback loop driven by procedural noise input.
local ParamSpec = require "paramspec"
return {
    name = "feedback_organic", version = 2, category = "VJ",  -- v2 = Sprint S8 Lot AE : material flow wired
    description = "Organic feedback: procedural noise + frame N-1 echo trail",
    tags = {"vj", "feedback", "noise", "procedural", "organic", "demo"},
    params = ParamSpec.declare({
        ParamSpec.float("decay",  0.92, {min=0,   max=1,    label="Decay"}),
        ParamSpec.float("scale",  4.0,  {min=0.5, max=16,   label="Noise Scale"}),
    }),
    build = function(params)
        return {
            type = "CompositionNode",
            primary = "feedback",
            nodes = {
                {name="cam",       type="CameraNode"},
                {name="light",     type="LightNode"},
                {name="mesh",      type="SceneObjectNode"},
                {name="noise",     type="NoiseTextureNode",
                  params={noise_type="perlin", scale=params.scale or 4.0}},
                {name="feedback",  type="TextureFeedbackNode",
                  params={decay=params.decay or 0.92, blend_mode="screen"}},
                {name="overlay",   type="FullscreenOverlayNode",
                  params={mode="screen_aligned"}},
                {name="color",     type="ColorShiftNode"},
                -- v3.5.2 Sprint S8 Lot AE : MaterialBridge route feedback.material_out → overlay.
                {name="bridge",    type="MaterialBridgeNode",
                  params={lighting_mode="unlit"}},
            },
            links = {
                {from="mesh", fromPort="entity", to="color", toPort="entity"},
                -- v3.5.2 Sprint S8 Lot AL : link noise → feedback.source_texture
                -- so the feedback material's TUS 0 receives a real texture (otherwise
                -- the 2 RTTs are empty/black and the overlay renders transparent).
                {from="noise", fromPort="texture_ready", to="feedback", toPort="source_texture"},
                -- Material flow : feedback (with noise as source) → overlay
                {from="feedback", fromPort="material_ready", to="bridge",  toPort="material_source"},
                {from="overlay",  fromPort="entity",         to="bridge",  toPort="entity"},
            },
            params = params,
        }
    end
}
