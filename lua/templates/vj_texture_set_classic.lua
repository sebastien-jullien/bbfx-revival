-- Template: VJ Texture Set Classic  (v3.5.2 Sprint S8 Lot AF — CDC OBJ-352-115)
-- Description: FullscreenOverlay + TextureCycle + TextureBlend + GamepadNode + JoystickRouter,
-- reproduction structurelle du préset Fanions historique 2006.
--   - Button 0 (press) = sweep next preset
--   - Button 1 (held)  = gate axis 0/1 → scroll U/V du blend
-- BPM: 144 (rate Fanions historique)
return {
    name = "VJ Texture Set Classic",
    bpm = 144,
    description = "Texture set classic VJ pattern (BBFx 2006 Fanions structure) — cycle + sweep blend + joystick scroll.",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(144) end

        -- ── Phase 1: all node creations ──
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode",  "light")
        dbg.create("TextureCycleNode", "cycle1")
        dbg.create("TextureCycleNode", "cycle2")
        dbg.create("TextureBlendNode", "blend")
        dbg.create("FullscreenOverlayNode", "overlay")
        dbg.material_bridge("bridge", "", "unlit")
        dbg.create("GamepadNode", "gamepad")
        dbg.create("JoystickRouterNode", "router_sweep")
        dbg.create("JoystickRouterNode", "router_scroll_u")
        dbg.create("JoystickRouterNode", "router_scroll_v")
        _dbg_process_pending()  -- flush creates BEFORE set_param/link

        -- ── Phase 2: param config ──
        dbg.set_param("cycle1", "textures",        "BumpyMetal.jpg;NMHollyBumps.png;RustySteel.jpg;Chrome.jpg")
        dbg.set_param("cycle1", "transition_time", "1.0")
        dbg.set_param("cycle2", "textures",        "NMBalls.png;atheneNormalMap.png;clouds.jpg;dirt01.jpg")
        dbg.set_param("cycle2", "transition_time", "1.0")
        dbg.set_param("blend",  "tex_a",      "BumpyMetal.jpg")
        dbg.set_param("blend",  "tex_b",      "NMBalls.png")
        dbg.set_param("blend",  "mask",       "aureola.png")
        dbg.set_param("blend",  "blend_mode", "alpha")
        dbg.set_param("router_sweep",   "button_index", "0")
        dbg.set_param("router_sweep",   "mode",         "press_trigger")
        dbg.set_param("router_scroll_u","button_index", "1")
        dbg.set_param("router_scroll_u","axis_index",   "0")
        dbg.set_param("router_scroll_u","mode",         "hold_gate")
        dbg.set_param("router_scroll_v","button_index", "1")
        dbg.set_param("router_scroll_v","axis_index",   "1")
        dbg.set_param("router_scroll_v","mode",         "hold_gate")

        -- ── Phase 3: links ──
        dbg.link("blend",          "material_ready", "bridge",          "material_source")
        dbg.link("overlay",        "entity",         "bridge",          "entity")
        dbg.link("gamepad",        "buttonA",        "router_sweep",    "button")
        dbg.link("router_sweep",   "trigger",        "cycle1",          "next")
        dbg.link("router_sweep",   "trigger",        "cycle2",          "next")
        dbg.link("gamepad",        "buttonB",        "router_scroll_u", "button")
        dbg.link("gamepad",        "leftStickX",     "router_scroll_u", "axis")
        dbg.link("router_scroll_u","gated_value",    "blend",           "scroll_u_a")
        dbg.link("gamepad",        "buttonB",        "router_scroll_v", "button")
        dbg.link("gamepad",        "leftStickY",     "router_scroll_v", "axis")
        dbg.link("router_scroll_v","gated_value",    "blend",           "scroll_v_a")
    end
}
