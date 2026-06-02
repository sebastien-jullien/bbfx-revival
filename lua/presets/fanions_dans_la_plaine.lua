-- ─────────────────────────────────────────────────────────────────────
-- Preset: fanions_dans_la_plaine
-- Reproduction stricte de la set BBFx 2006 "Fanions dans la plaine"
-- (cf. prog/workspace/bbfx/bbfx/src/lua/0.1/setFanions.textures.lua)
--
-- Mecanisme :
--   - FullscreenOverlayNode (screen_aligned) — quad plein ecran (Rectangle2D NDC,
--     RENDER_QUEUE_OVERLAY). NB : le mode camera_locked d'origine 2006 (BillboardSet
--     colle camera) a ete retire (D14) car il ne rendait pas ; le rendu screen-aligned
--     couvre la viewport de maniere visuellement equivalente.
--   - TextureBlendNode (3 TUS : tex_a + tex_b + mask vertical sweep)
--   - 2 x TextureCycleNode (cycle1 et cycle2 — equivalent du `mux()` 2006)
--   - GamepadNode + 2 x JoystickRouterNode (S2 - Lot I)
--     - router_sweep  : button 0, mode press_trigger -> cycle.next
--     - router_scroll : button 1, mode hold_gate -> blend.scroll_u/v
--
-- Fidelite 2006 (post-S2) :
--   - Bouton 0 (presse) -> sweep next preset (TextureCycle.next via trigger)
--   - Bouton 1 (maintenu) -> axes du joystick gates pilotent scroll U/V
--   - Limitation S1 (axes always-on) RELEVEE par JoystickRouterNode
-- ─────────────────────────────────────────────────────────────────────

local ParamSpec = require "paramspec"

return {
    name = "fanions_dans_la_plaine",
    version = 3,  -- v3 = Sprint S8 Lot AE : material routing fixé (cycle → blend → overlay via MaterialBridge)
    category = "VJ",
    description = "Fanions dans la plaine — reproduction stricte set 2006 : 10 paires gray/color + sweep masque + joystick gating fidele",
    tags = {"vj", "2006", "fanions", "heritage", "texture_cycle", "joystick", "killer"},
    params = ParamSpec.declare({
        ParamSpec.float("transition_time", 1.0, {min=0.1, max=10, label="Transition Time (s)"}),
        ParamSpec.float("scroll_amplitude", 0.1, {min=0, max=1, label="Scroll Amplitude"}),
    }),
    build = function(params)
        -- 10 paires gray/color analogues a setFanions.textures.lua:351-362.
        -- v3.5.2 Sprint S7 Lot Y : preferer le Heritage Pack (manifest charge
        -- via bbfx.assets.load_pack a l'init Studio) et tomber gracieusement
        -- sur les textures bundled v3.5.1 si le manifest est vide (cas user
        -- qui n'a pas couru `tools/asset_pipeline.py` localement).
        local heritage_color = {
            "ambientcg_bark004",      "ambientcg_metal043a",
            "polyhaven_brown_planks_03", "ambientcg_marble006",
            "polyhaven_concrete_layers", "ambientcg_pavingstones072",
            "ambientcg_paintedmetal003", "polyhaven_factory_brick",
            "ambientcg_rock035",      "polyhaven_mossy_cobblestone",
        }
        local fallback_color = {
            "BumpyMetal.jpg",   "NMHollyBumps.png", "RustySteel.jpg",
            "NMBalls.png",      "Chrome.jpg",       "atheneNormalMap.png",
            "clouds.jpg",       "dirt01.jpg",       "Water01.jpg",
            "Water02.jpg",
        }
        -- bbfx.assets est expose meme si vide. entry_count==0 = pas de manifest
        -- charge => fallback v3.5.1.
        local use_heritage = (bbfx and bbfx.assets and bbfx.assets.entry_count
                              and bbfx.assets.entry_count() >= 10)
        local pool, head_a, head_b
        if use_heritage then
            pool = {}
            for _, n in ipairs(heritage_color) do
                local fn = bbfx.assets.resolve(n)
                if fn ~= "" then table.insert(pool, fn) end
            end
            -- Si 10 noms Heritage choisis ne sont pas tous dans le manifest
            -- (ex. user a un sous-set), bascule fallback pour ne pas casser.
            if #pool < 10 then use_heritage = false end
        end
        if not use_heritage then
            pool = fallback_color
        end
        -- cycle2 = rotation de cycle1 pour creer le sweep dual-set du 2006.
        local function rotate(t, n)
            local r = {}
            for i = 1, #t do r[i] = t[((i - 1 + n) % #t) + 1] end
            return r
        end
        local cycle1_list = pool
        local cycle2_list = rotate(pool, 5)
        local function csv(t) return table.concat(t, ";") end
        local cycle1_textures = csv(cycle1_list)
        local cycle2_textures = csv(cycle2_list)
        local blend_tex_a = cycle1_list[1]
        local blend_tex_b = cycle2_list[1]

        return {
            type = "CompositionNode",
            primary = "fullscreen_overlay",
            nodes = {
                {name="cam",                type="CameraNode"},
                {name="light",              type="LightNode"},
                {name="cycle1",             type="TextureCycleNode",
                  params={textures=cycle1_textures, transition_time=params.transition_time or 1.0}},
                {name="cycle2",             type="TextureCycleNode",
                  params={textures=cycle2_textures, transition_time=params.transition_time or 1.0}},
                {name="blend",              type="TextureBlendNode",
                  params={tex_a=blend_tex_a, tex_b=blend_tex_b,
                          mask="aureola.png", blend_mode="alpha"}},
                {name="fullscreen_overlay", type="FullscreenOverlayNode",
                  params={mode="screen_aligned"}},
                -- v3.5.2 Sprint S8 Lot AE : MaterialBridge route blend.material_out
                -- vers le ParamSpec.material du FullscreenOverlay (Pattern 3 dual :
                -- bridge avec target FullscreenOverlay grace au support multi-target
                -- ajoute en Lot AC).
                {name="bridge",             type="MaterialBridgeNode",
                  params={lighting_mode="unlit"}},
                {name="gamepad",            type="GamepadNode"},
                -- S2 Lot I : JoystickRouter releve la limitation S1
                {name="router_sweep",       type="JoystickRouterNode",
                  params={button_index=0, axis_index=0, mode="press_trigger"}},
                {name="router_scroll_u",    type="JoystickRouterNode",
                  params={button_index=1, axis_index=0, mode="hold_gate"}},
                {name="router_scroll_v",    type="JoystickRouterNode",
                  params={button_index=1, axis_index=1, mode="hold_gate"}},
            },
            links = {
                -- v3.5.2 Sprint S8 Lot AE — Material flow chain :
                --   TextureBlend → MaterialBridge → FullscreenOverlay
                -- Le bridge consomme `material_out` via `material_source` Pattern 3
                -- et l'applique sur le ParamSpec.material du FullscreenOverlay.
                {from="blend",              fromPort="material_ready", to="bridge",             toPort="material_source"},
                {from="fullscreen_overlay", fromPort="entity",         to="bridge",             toPort="entity"},

                -- Gamepad button A (button_index=0) -> router_sweep -> cycle1.next + cycle2.next
                {from="gamepad",      fromPort="buttonA",       to="router_sweep",    toPort="button"},
                {from="router_sweep", fromPort="trigger",       to="cycle1",          toPort="next"},
                {from="router_sweep", fromPort="trigger",       to="cycle2",          toPort="next"},

                -- Gamepad button B (button_index=1) maintenu = gating ; axis 0/1 = scroll U/V
                {from="gamepad",        fromPort="buttonB",     to="router_scroll_u", toPort="button"},
                {from="gamepad",        fromPort="leftStickX",  to="router_scroll_u", toPort="axis"},
                {from="router_scroll_u", fromPort="gated_value", to="blend",          toPort="scroll_u_a"},
                {from="router_scroll_u", fromPort="gated_value", to="blend",          toPort="scroll_u_b"},

                {from="gamepad",        fromPort="buttonB",     to="router_scroll_v", toPort="button"},
                {from="gamepad",        fromPort="leftStickY",  to="router_scroll_v", toPort="axis"},
                {from="router_scroll_v", fromPort="gated_value", to="blend",          toPort="scroll_v_a"},
                {from="router_scroll_v", fromPort="gated_value", to="blend",          toPort="scroll_v_b"},
            },
            params = params,
        }
    end
}
