-- Preset: vj_complete_show
-- Sprint S4 final demo: complete VJ set wiring video / texture / feedback /
-- spectrogram / audio-react / gamepad. Showcases all v3.5.2 killer features
-- in a single graph (~14 nodes — close to the 25+ target once Heritage
-- Pack assets are sourced and the spectrogram is wired to a vertex shader).
local ParamSpec = require "paramspec"
return {
    name = "vj_complete_show", version = 1, category = "VJ",
    description = "Complete VJ show: video crossfade + texture set + feedback + spectrogram + audio-react",
    tags = {"vj", "complete_show", "audio", "video", "texture", "feedback", "spectrogram", "killer", "demo"},
    params = ParamSpec.declare({}),
    build = function(params)
        local presets_csv = "BumpyMetal.jpg|Water01.jpg;"
                         .. "Chrome.jpg|atheneNormalMap.png;"
                         .. "RustySteel.jpg|clouds.jpg"
        return {
            type = "CompositionNode",
            primary = "overlay_video",
            nodes = {
                -- 3D scene
                {name="cam",          type="CameraNode"},
                {name="light",        type="LightNode"},
                {name="mesh",         type="SceneObjectNode"},

                -- Audio reactivity
                {name="audio",        type="AudioCaptureNode"},
                {name="spectro",      type="SpectrogramTextureNode",
                  params={colormap="plasma"}},

                -- Two-clip video crossfade
                {name="clip_a",       type="TheoraClipNode"},
                {name="clip_b",       type="TheoraClipNode"},
                {name="xfade",        type="VideoCrossfadeNode"},

                -- Texture set on top
                {name="bank",         type="MultiTextureBankNode",
                  params={presets=presets_csv, slot_count=2}},
                {name="blend",        type="TextureBlendNode",
                  params={blend_mode="screen", mask="aureola.png"}},

                -- Feedback layer
                {name="feedback",     type="TextureFeedbackNode",
                  params={decay=0.85, blend_mode="additive"}},

                -- Two stacked overlays (video bg + textured fg)
                {name="overlay_video", type="FullscreenOverlayNode",
                  params={mode="screen_aligned", z_offset=0.02}},
                {name="overlay_tex",   type="FullscreenOverlayNode",
                  params={mode="screen_aligned", z_offset=0.01}},

                -- v3.5.2 Sprint S8 Lot AE : MaterialBridges relient les producteurs
                -- (xfade / blend) vers les ParamSpec.material des overlays via
                -- Pattern 3 + extension multi-target Lot AC.
                {name="bridge_video", type="MaterialBridgeNode",
                  params={lighting_mode="unlit"}},
                {name="bridge_tex",   type="MaterialBridgeNode",
                  params={lighting_mode="unlit"}},

                -- Input routing
                {name="gamepad",      type="GamepadNode"},
                {name="router_xfade", type="JoystickRouterNode",
                  params={button_index=0, axis_index=0, mode="hold_gate"}},
                {name="router_next",  type="JoystickRouterNode",
                  params={button_index=1, axis_index=0, mode="press_trigger"}},
            },
            links = {
                {from="clip_a",        fromPort="playing",      to="xfade",        toPort="clip_a"},
                {from="clip_b",        fromPort="playing",      to="xfade",        toPort="clip_b"},
                {from="gamepad",       fromPort="leftStickX",   to="router_xfade", toPort="axis"},
                {from="gamepad",       fromPort="buttonA",      to="router_xfade", toPort="button"},
                {from="router_xfade",  fromPort="gated_value",  to="xfade",        toPort="beta"},

                {from="gamepad",       fromPort="buttonB",      to="router_next",  toPort="button"},
                {from="router_next",   fromPort="trigger",      to="bank",         toPort="next_preset"},

                {from="audio",         fromPort="frame_ready",  to="spectro",      toPort="audio"},

                -- v3.5.2 Sprint S8 Lot AE — DAG material flow :
                --   xfade.material_out ⟶ bridge_video ⟶ overlay_video.material
                --   blend.material_out ⟶ bridge_tex   ⟶ overlay_tex.material
                {from="xfade",         fromPort="material_ready", to="bridge_video", toPort="material_source"},
                {from="overlay_video", fromPort="entity",         to="bridge_video", toPort="entity"},
                {from="blend",         fromPort="material_ready", to="bridge_tex",   toPort="material_source"},
                {from="overlay_tex",   fromPort="entity",         to="bridge_tex",   toPort="entity"},
            },
            params = params,
        }
    end
}
