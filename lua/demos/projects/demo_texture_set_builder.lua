-- demo_texture_set_builder.lua — BBFx v3.5.2 Sprint S8 Lot AV.2 (Plan B fix RTSS + repro 2006 fidèle)
--
-- Repro **fidèle** du « Texture Set » 2006 (Fanions — cf. prog/.../lua/0.1/textureset.lua).
--
-- Sémantique 2006 (`TextureSet:new`) :
--   - un cycle de COUPLES `{gray, color, factor}` qu'on avance via `next()` ;
--   - sur chaque preset, layer A reçoit `gray`, layer B reçoit `color`, et `factor`
--     multiplie la vitesse de scroll de B (négatif → sens opposé → motifs de croisement) ;
--   - vitesses bornées par `setSpeedBounds(-2u, +2u, -2v, +2v)` ;
--   - un `scrollbutton` modal : bouton tenu = stick gauche pilote `uSpeed/vSpeed` des
--     deux couches (gates les axes 0 et 1 en mode HoldGate) ; relâché = vitesses figées.
--
-- Implémentation v3.5.2 (post-Lot AV.2) :
--   - **`FullscreenOverlayNode` mode `screen_aligned`** (Rectangle2D NDC plein écran +
--     RENDER_QUEUE_OVERLAY) — c'est le rendu plein-cadre que voulait l'utilisateur.
--     Le bug I-2050 (mutations TUS figées par RTSS post-bind) est fixé en Lot AV.2 par
--     `ShaderGenerator::invalidateMaterial` après chaque mutation TUS dans `TextureBlendNode`.
--   - `TextureSetNode` (Lot AV) : un seul `next` avance le couple GRAY+COLOR+FACTOR ;
--   - `TextureBlendNode` (Lot AV) : ports `factor` (× layer B), `u_amp` / `v_amp`
--     (bornes ± reproduisant `setSpeedBounds` 2006) ;
--   - `JoystickRouterNode` mode `scroll_gate` (Lot AV) = alias 2006 du `hold_gate`.
--
-- Mapping manette (cf. textureset.lua:170-191) :
--   - bouton A (idx 0) press_trigger → `tset.next` (couple suivant)
--   - bouton B (idx 1) scrollbutton tenu, stick gauche → vitesses U/V des deux couches
--   - bouton X (idx 2) toggle → enable/disable de l'overlay
--
-- Sans manette : `dbg.set("tset","next",1)` avance le couple ;
--               `dbg.set("blend","scroll_u_a_speed",0.15)` pilote la vitesse U manuellement.
--
-- 144 BPM, presets = 3 couples gray/color/factor.

return {
    name = "Texture Set Classic (Fanions 2006, fidèle, fix RTSS)",
    bpm  = 144,
    description = "Repro 2006 fidèle : overlay plein écran Rectangle2D, couples {gray, color, factor} "
               .. "cycliques via TextureSetNode, factor multiplie scroll layer B (négatif = sens opposé), "
               .. "scrollbutton tenu = stick pilote les vitesses des deux couches, bornes ±u_amp/v_amp. "
               .. "Bug I-2050 (RTSS material freeze) fixé par invalidateMaterial dans TextureBlendNode.",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(144) end

        -- ── Nodes ──
        dbg.create("FullscreenOverlayNode", "overlay")     -- Rectangle2D NDC plein écran
        dbg.create("TextureBlendNode",      "blend")       -- composite layer A + masque + layer B
        dbg.create("TextureSetNode",        "tset")        -- cycle des couples 2006

        dbg.create("GamepadNode",        "gamepad")
        dbg.create("JoystickRouterNode", "rt_next")        -- A → tset.next (press_trigger)
        dbg.create("JoystickRouterNode", "rt_scroll_u")    -- B tenu + stickX → vitesse U
        dbg.create("JoystickRouterNode", "rt_scroll_v")    -- B tenu + stickY → vitesse V
        dbg.create("JoystickRouterNode", "rt_hide")        -- X toggle → overlay on/off
        dbg.create("MathNode",           "inv_hide")       -- 1 - rt_hide.toggled → visible
        _dbg_process_pending()

        -- ── Overlay plein écran (= ce que voulait 2006 avec son BillboardSet camera-locked).
        dbg.set_param("overlay", "mode", "screen_aligned")
        dbg.set("overlay", "alpha", 1.0)

        -- ── Blend (composite 3-TUS) ──
        dbg.set_param("blend", "tex_a", "BumpyMetal.jpg")  -- fallbacks visuels avant 1er tick
        dbg.set_param("blend", "tex_b", "RustySteel.jpg")
        dbg.set_param("blend", "mask",  "aureola.png")
        dbg.set_param("blend", "blend_mode", "alpha")
        -- Bornes 2006 (« setSpeedBounds(-2u, +2u, -2v, +2v) » avec u ≈ 0.1) → ±0.2 U, ±0.1 V.
        dbg.set("blend", "u_amp", 0.2)
        dbg.set("blend", "v_amp", 0.1)
        -- Vitesse de fond "ronflement" du masque (vsweep continu façon 2006).
        dbg.set("blend", "mask_offset_v_speed", 0.20)

        -- ── TextureSet : 3 couples gray/color/factor ──
        -- Le facteur ≈ -0.7 (textureset.lua usage typique) inverse + ralentit le scroll layer B.
        dbg.set_param("tset", "presets",
            "BumpyMetal.jpg|RustySteel.jpg|-0.7;"
         .. "Chrome.jpg|Water01.jpg|-0.7;"
         .. "clouds.jpg|BumpyMetal.jpg|-0.5")
        dbg.set_param("tset", "transition_time", "1.0")

        -- ── Routers manette ──
        -- A (idx 0) press_trigger → next du Set.
        dbg.set_param("rt_next",     "button_index", "0")
        dbg.set_param("rt_next",     "mode",         "press_trigger")

        -- B (idx 1) scrollbutton : tenu + axe stick = vitesse texture (mode scroll_gate Lot AV).
        dbg.set_param("rt_scroll_u", "button_index", "1")
        dbg.set_param("rt_scroll_u", "axis_index",   "0")    -- leftStickX
        dbg.set_param("rt_scroll_u", "mode",         "scroll_gate")

        dbg.set_param("rt_scroll_v", "button_index", "1")
        dbg.set_param("rt_scroll_v", "axis_index",   "1")    -- leftStickY
        dbg.set_param("rt_scroll_v", "mode",         "scroll_gate")

        -- X (idx 2) toggle → enable/disable de l'overlay via MathNode inverseur `inv_hide`.
        -- inv_hide.out = a - b = 1 - rt_hide.toggled. À frame 1, rt_hide.toggled=0 (initial),
        -- inv_hide.update calcule out=1 et la propagation pousse 1 sur overlay.visible AVANT
        -- que overlay.update lise sa port `visible` (BFS via time.dt → ... → overlay).
        dbg.set_param("rt_hide",     "button_index", "2")
        dbg.set_param("rt_hide",     "mode",         "toggle")

        -- inv_hide : subtract, a=1, b lié à rt_hide.toggled.
        dbg.set("inv_hide", "operation", 1.0)
        dbg.set("inv_hide", "a",         1.0)

        -- ── Links ──
        -- dt sur intégrateurs et transitions.
        dbg.link("time", "dt", "blend", "dt")
        dbg.link("time", "dt", "tset",  "dt")

        -- TextureSet → Blend (textures synchronisées + factor).
        dbg.link("tset", "texture_a_ready", "blend", "tex_a_source")
        dbg.link("tset", "texture_b_ready", "blend", "tex_b_source")
        dbg.link("tset", "factor",          "blend", "factor")

        -- Blend → Overlay plein écran (FSO Pattern 3 consumer pull `material_out`).
        dbg.link("blend", "material_ready", "overlay", "material_source")

        -- Gamepad → routers.
        dbg.link("gamepad", "buttonA", "rt_next",     "gamepad")
        dbg.link("gamepad", "buttonB", "rt_scroll_u", "gamepad")
        dbg.link("gamepad", "buttonB", "rt_scroll_v", "gamepad")
        dbg.link("gamepad", "buttonX", "rt_hide",     "gamepad")

        -- Routers → cibles.
        dbg.link("rt_next",     "trigger",     "tset",  "next")
        -- Scrollbutton tenu = stick X/Y → vitesses des couches (le `factor` du Set
        -- module ensuite layer B en sens opposé à l'intérieur de blend).
        dbg.link("rt_scroll_u", "gated_value", "blend", "scroll_u_a_speed")
        dbg.link("rt_scroll_u", "gated_value", "blend", "scroll_u_b_speed")
        dbg.link("rt_scroll_v", "gated_value", "blend", "scroll_v_a_speed")
        dbg.link("rt_scroll_v", "gated_value", "blend", "scroll_v_b_speed")

        -- Bouton X = toggle on/off de l'overlay via inv_hide.
        dbg.link("rt_hide",  "toggled", "inv_hide", "b")
        dbg.link("inv_hide", "out",     "overlay",  "visible")

        -- N.B. : on n'amorce PAS `tset.next=1` ici (l'ancienne version le faisait, mais
        -- ça met `mPrevNextState=true` pour toute la durée où aucun input gamepad ne
        -- réveille la propagation rt_next.trigger → tset.next, bloquant le 1ᵉʳ vrai
        -- press du bouton A. La démo démarre donc sur le 1ᵉʳ preset `BumpyMetal/RustySteel`
        -- et attend une pression A pour passer au 2ᵉ preset `Chrome/Water01`.
    end
}
