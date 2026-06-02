-- Template: VJ Hybrid 3D + Overlay  (v3.5.2 Sprint S8 Lot AF — CDC OBJ-352-116)
-- Description: Scène 3D (Geosphere + PerlinFx vertex deform) + FullscreenOverlay video
-- par-dessus en blend additif (alpha 0.4) → mix 3D + vidéo plein écran.
-- BPM: 128
return {
    name = "VJ Hybrid 3D + Overlay",
    bpm = 128,
    description = "Hybrid scene: 3D geosphere with Perlin vertex deformation + video overlay screen-blended on top.",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(128) end

        -- ── Phase 1 : all creates ──
        dbg.create("CameraNode", "cam")
        dbg.create("LightNode",  "light")
        dbg.create_with_param("SceneObjectNode", "geo",   "mesh", "Geosphere8000.mesh")
        dbg.create("PerlinFxNode", "deform")
        dbg.create("TheoraClipNode", "video")
        dbg.create("FullscreenOverlayNode", "overlay")
        dbg.material_bridge("bridge", "", "unlit")
        _dbg_process_pending()  -- flush creates BEFORE set_param/link

        -- ── Phase 2 : param config ──
        dbg.set_param("overlay", "mode",     "camera_locked")
        dbg.set_param("overlay", "z_offset", "0.02")
        -- Alpha 0.4 (transparent → 3D scene visible through)
        if dbg.set_port then dbg.set_port("overlay", "alpha", 0.4) end

        -- ── Phase 3 : links ──
        dbg.link("geo", "entity", "deform", "entity")
        -- Material flow : video.material_out → bridge → overlay.material
        dbg.link("video",   "material_ready", "bridge",  "material_source")
        dbg.link("overlay", "entity",         "bridge",  "entity")
    end
}
