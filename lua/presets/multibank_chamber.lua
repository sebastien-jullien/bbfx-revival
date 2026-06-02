-- Preset: multibank_chamber
-- Sprint S4 demo: MultiTextureBank (5 presets x 2 slots) + TextureBlend + FullscreenOverlay
-- Demonstrates indexed paired-texture banks (gray/color) routed via TextureBlend.
local ParamSpec = require "paramspec"
return {
    name = "multibank_chamber", version = 1, category = "VJ",
    description = "Multi-bank texture chamber: 5 paired presets switchable via gamepad",
    tags = {"vj", "multi_texture", "bank", "demo"},
    params = ParamSpec.declare({}),
    build = function(params)
        -- 5 presets x 2 slots, encoded as '|'-cols ';'-rows for the
        -- MultiTextureBankNode parser (cf. textures bundled with v3.5.x).
        local presets_csv = "BumpyMetal.jpg|Water01.jpg;"
                         .. "Chrome.jpg|Water02.jpg;"
                         .. "RustySteel.jpg|atheneNormalMap.png;"
                         .. "NMBalls.png|clouds.jpg;"
                         .. "NMHollyBumps.png|dirt01.jpg"
        return {
            type = "CompositionNode",
            primary = "overlay",
            nodes = {
                {name="cam",     type="CameraNode"},
                {name="bank",    type="MultiTextureBankNode",
                  params={presets=presets_csv, slot_count=2, mode="sequential"}},
                {name="blend",   type="TextureBlendNode",
                  params={tex_a="BumpyMetal.jpg", tex_b="Water01.jpg",
                          mask="aureola.png", blend_mode="alpha"}},
                {name="overlay", type="FullscreenOverlayNode",
                  params={mode="screen_aligned"}},
                -- v3.5.2 Sprint S8 Lot AE : MaterialBridge route blend.material_out
                -- vers le FullscreenOverlay (Pattern 3 + multi-target Lot AC).
                {name="bridge",  type="MaterialBridgeNode",
                  params={lighting_mode="unlit"}},
                {name="gamepad", type="GamepadNode"},
                {name="router",  type="JoystickRouterNode",
                  params={button_index=0, axis_index=0, mode="press_trigger"}},
            },
            links = {
                {from="gamepad", fromPort="buttonA", to="router",  toPort="button"},
                {from="router",  fromPort="trigger", to="bank",    toPort="next_preset"},
                -- v3.5.2 Sprint S8 Lot AE — DAG material flow chain :
                --   bank.slot_*_texture (mirrors)  ⟶ blend.tex_a / tex_b (static via param,
                --                                    refreshed each preset switch by user wiring)
                --   blend.material_out ⟶ bridge.material_source ⟶ overlay.material
                {from="blend",   fromPort="material_ready", to="bridge",  toPort="material_source"},
                {from="overlay", fromPort="entity",         to="bridge",  toPort="entity"},
            },
            params = params,
        }
    end
}
