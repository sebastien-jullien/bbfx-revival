-- Template: VJ Dual Video Crossfade  (v3.5.2 Sprint S8 Lot AF — CDC OBJ-352-114)
-- Description: 2 TheoraClips + VideoCrossfadeNode + FullscreenOverlay + Gamepad + JoystickRouter.
--   - Button 0 (press) = next clip A (cycle clips via VideoLibrary or manual swap)
--   - Button 1 (press) = next clip B
--   - Axis 0          = beta crossfade A↔B
-- BPM: 120
return {
    name = "VJ Dual Video Crossfade",
    bpm = 120,
    description = "Live dual-video crossfade for VJ performance — gamepad-driven beta + per-channel next-trigger.",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end

        -- ── Phase 1 : all creates ──
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode",  "light")
        dbg.create("TheoraClipNode", "clip_a")
        dbg.create("TheoraClipNode", "clip_b")
        dbg.create("VideoCrossfadeNode", "xfade")
        dbg.create("FullscreenOverlayNode", "overlay")
        dbg.material_bridge("bridge", "", "unlit")
        dbg.create("GamepadNode", "gamepad")
        dbg.create("JoystickRouterNode", "router_beta")
        dbg.create("JoystickRouterNode", "router_next_a")
        dbg.create("JoystickRouterNode", "router_next_b")
        _dbg_process_pending()  -- flush creates BEFORE set_param/link

        -- ── Phase 2 : param config ──
        dbg.set_param("router_beta",   "button_index", "0")
        dbg.set_param("router_beta",   "axis_index",   "0")
        dbg.set_param("router_beta",   "mode",         "hold_gate")
        dbg.set_param("router_next_a", "button_index", "0")
        dbg.set_param("router_next_a", "mode",         "press_trigger")
        dbg.set_param("router_next_b", "button_index", "1")
        dbg.set_param("router_next_b", "mode",         "press_trigger")

        -- ── Phase 3 : links ──
        dbg.link("clip_a",        "playing",        "xfade",         "clip_a")
        dbg.link("clip_b",        "playing",        "xfade",         "clip_b")
        dbg.link("gamepad",       "leftStickX",     "router_beta",   "axis")
        dbg.link("gamepad",       "buttonA",        "router_beta",   "button")
        dbg.link("router_beta",   "gated_value",    "xfade",         "beta")
        -- Material flow : xfade.material_out → bridge → overlay.material (multi-target).
        dbg.link("xfade",         "material_ready", "bridge",        "material_source")
        dbg.link("overlay",       "entity",         "bridge",        "entity")
    end
}
