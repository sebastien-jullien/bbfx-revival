-- demo_video_wall_builder.lua — BBFx v3.5.2 Sprint S8 Lot AV.4 round 4 (2026-05-16)
--
-- Showcase **propre et fidèle 2006** des capacités vidéo. UN SEUL TheoraClipNode
-- avec un 2ᵉ stream pré-rendu inversé chargé en interne via le param
-- `reverse_filename`. Le bouton B swap atomiquement le reader actif (= la repro
-- exacte de `Ogre::ReversableClip::doReverse()` 2006, cf. prog/workspace/test-theora).
--
-- Architecture :
--   - `FullscreenOverlayNode` mode `screen_aligned` (Rectangle2D NDC plein cadre).
--   - 1 `TheoraClipNode "clip"` qui charge `bombe.ogg` (forward) + `bombe_reverse.ogg`
--     (reverse, pré-rendu via ffmpeg -vf reverse). UN SEUL thread de décodage actif
--     à la fois — c'est le swap de pointeur dans TheoraClip qui change la source
--     du décodage, pas un 2ᵉ thread en parallèle. FPS conservés (vs 3-4 fps avec
--     l'approche 2 clips parallèles + VideoCrossfade du round 2).
--   - Routers + MathNodes pour les contrôles manette.
--
-- Mapping manette :
--   - Bouton A (Cross / South idx 0) toggle : PLAY / PAUSE (via `inv_play 1-x`).
--   - Bouton B (Circle / East idx 1) toggle : REVERSE direction. rt_rev.toggled
--     branche directement sur `clip.reverse` (port edge-détecté). 0 → forward,
--     1 → reverse stream. Visuellement la vidéo joue à l'envers parce que le 2ᵉ
--     stream est pré-rendu reversed.
--   - Bouton X (Square / West idx 2) toggle : MASQUER / AFFICHER l'overlay
--     (modèle texture_set, inv_hide = 1 - x → overlay.visible).
--   - Right Trigger (R2) continu : accélération 1×→4× (lerp).
--   - Left Trigger (L2) continu : ralentissement 1×→0.2× (lerp).
--   - RT et LT combinés multiplicativement → clip.speed.
--
-- Sans manette : `dbg.set("clip","reverse",1)` joue à l'envers ; `dbg.set("clip","speed",0.5)` etc.
--
-- 120 BPM.

return {
    name = "Video Wall (Theora plein écran + reverse + gamepad)",
    bpm  = 120,
    description = "Overlay plein-écran. Bouton A=play/pause, B=reverse (swap reader 2006 ReversableClip), "
               .. "X=hide, R2=accélérer 1→4×, L2=ralentir 1→0.2×. UN SEUL décodage actif → FPS OK.",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end

        -- ── Nodes ──
        dbg.create("FullscreenOverlayNode", "overlay")
        dbg.create_with_param("TheoraClipNode", "clip", "filename", "resources/video/bombe.ogg")

        dbg.create("GamepadNode",        "gamepad")
        dbg.create("JoystickRouterNode", "rt_play")   -- A toggle → play/pause
        dbg.create("JoystickRouterNode", "rt_rev")    -- B toggle → reverse
        dbg.create("JoystickRouterNode", "rt_hide")   -- X toggle → overlay on/off
        dbg.create("MathNode",           "inv_play")  -- 1 - rt_play.toggled → clip.play
        dbg.create("MathNode",           "inv_hide")  -- 1 - rt_hide.toggled → overlay.visible
        dbg.create("MathNode",           "ff_speed")  -- lerp(1, 4, rightTrigger) → fast 1×→4×
        dbg.create("MathNode",           "sm_speed")  -- lerp(1, 0.2, leftTrigger)  → slow 1×→0.2×
        dbg.create("MathNode",           "mix_speed") -- ff_speed × sm_speed = vitesse finale
        _dbg_process_pending()

        -- ── Overlay plein écran ──
        dbg.set_param("overlay", "mode", "screen_aligned")
        dbg.set("overlay", "alpha", 1.0)

        -- ── Clip : charge le reverse stream pré-rendu (généré offline via ffmpeg -vf reverse).
        --    Le TheoraClipNode.update() lazy-load le 2ᵉ TheoraReader la 1ʳᵉ frame où ce param
        --    est non vide. Le bouton B flip ensuite le pointeur `mActiveReader` côté C++.
        dbg.set_param("clip", "reverse_filename", "resources/video/bombe_reverse.ogg")

        -- ── Routers ──
        dbg.set_param("rt_play", "button_index", "0"); dbg.set_param("rt_play", "mode", "toggle")
        dbg.set_param("rt_rev",  "button_index", "1"); dbg.set_param("rt_rev",  "mode", "toggle")
        dbg.set_param("rt_hide", "button_index", "2"); dbg.set_param("rt_hide", "mode", "toggle")

        -- ── Math nodes ──
        dbg.set("inv_play", "operation", 1.0); dbg.set("inv_play", "a", 1.0)
        dbg.set("inv_hide", "operation", 1.0); dbg.set("inv_hide", "a", 1.0)
        dbg.set("ff_speed", "operation", 10.0); dbg.set("ff_speed", "a", 1.0); dbg.set("ff_speed", "b", 4.0)
        dbg.set("sm_speed", "operation", 10.0); dbg.set("sm_speed", "a", 1.0); dbg.set("sm_speed", "b", 0.2)
        dbg.set("mix_speed", "operation", 2.0)

        -- ── Links ──
        dbg.link("time", "dt", "clip", "dt")

        -- Clip material → overlay plein écran (Pattern 3 consumer pull `material_out`).
        dbg.link("clip", "material_ready", "overlay", "material_source")

        -- Gamepad → routers.
        dbg.link("gamepad", "buttonA", "rt_play", "gamepad")
        dbg.link("gamepad", "buttonB", "rt_rev",  "gamepad")
        dbg.link("gamepad", "buttonX", "rt_hide", "gamepad")

        -- A → play/pause.
        dbg.link("rt_play",  "toggled", "inv_play", "b")
        dbg.link("inv_play", "out",     "clip",     "play")

        -- B → reverse (direct, le port reverse est edge-détecté côté TheoraClipNode).
        dbg.link("rt_rev",   "toggled", "clip",     "reverse")

        -- X → hide/show overlay.
        dbg.link("rt_hide",  "toggled", "inv_hide", "b")
        dbg.link("inv_hide", "out",     "overlay",  "visible")

        -- R2 accélère, L2 ralentit, combinés.
        dbg.link("gamepad",   "rightTrigger", "ff_speed",  "t")
        dbg.link("gamepad",   "leftTrigger",  "sm_speed",  "t")
        dbg.link("ff_speed",  "out",          "mix_speed", "a")
        dbg.link("sm_speed",  "out",          "mix_speed", "b")
        dbg.link("mix_speed", "out",          "clip",      "speed")
    end
}
