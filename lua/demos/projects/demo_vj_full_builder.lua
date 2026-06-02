-- demo_vj_full_builder.lua — BBFx v3.5.2 Sprint S8 Lot AU (I-2011)
--
-- Showcase : la scène "tout BBFx" — meshes 3D + déformateurs + particules +
-- vidéo backdrop + overlay plein écran + ColorShift + gamepad + beat-react.
-- C'est la scène-référence "regarde tout ce qu'on peut faire".
--   - geosphere déformée Perlin + knot ondulé WaveVertexShader (ColorShift hue)
--   - 2 ParticleNodes (Galaxy + Confetti pulsé au beat — Lot AT)
--   - TheoraClip → FullscreenOverlay backdrop camera_locked (alpha 0.5)
--   - TextureCycleNode → TextureBlendNode → MaterialBridge sur le knot (alt texture)
--   - GamepadNode + JoystickRouter : bouton bascule l'overlay backdrop on/off (Lot AT)
--   - CameraNode en orbit
--
-- Layout : caméra MOBILE (orbit) · vidéo ACCROCHÉE CAMÉRA (backdrop) · 120 BPM.

return {
    name = "Full VJ Show",
    bpm = 120,
    description = "Kitchen-sink VJ : geosphere Perlin + knot Wave/ColorShift + Galaxy/Confetti + vidéo backdrop + TextureBlend + gamepad toggle overlay — caméra orbit.",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end

        dbg.create("SceneObjectNode",       "geosphere")
        dbg.create("SceneObjectNode",       "knot")
        dbg.create("PerlinFxNode",          "perlin")
        dbg.create("WaveVertexShader",      "wave")
        dbg.create("ColorShiftNode",        "hue")
        dbg.create("MathNode",              "hue_anim")
        dbg.create("ParticleNode",          "galaxy")
        dbg.create("ParticleNode",          "confetti")
        dbg.create("BeatTriggerNode",       "beat_trig")
        dbg.create("TheoraClipNode",        "clip_bg")
        dbg.create("FullscreenOverlayNode", "backdrop")
        dbg.material_bridge("bridge_bg", "", "unlit")
        dbg.create("TextureCycleNode",      "cycle")
        dbg.create("TextureBlendNode",      "blend")
        dbg.material_bridge("bridge_knot", "", "lit")
        dbg.create("GamepadNode",           "gamepad")
        dbg.create("JoystickRouterNode",    "router_toggle")
        dbg.create("CameraNode",            "cam")
        dbg.create("LightNode",             "light_key")
        dbg.create("LightNode",             "light_fill")
        _dbg_process_pending()

        dbg.set_param("geosphere", "mesh_file", "geosphere4500.mesh"); dbg.set_param("geosphere","material","BaseWhiteNoLighting")
        dbg.set("geosphere","position.y",20.0)
        dbg.set("geosphere","scale.x",0.05); dbg.set("geosphere","scale.y",0.05); dbg.set("geosphere","scale.z",0.05)
        dbg.set_param("knot", "mesh_file", "knot.mesh"); dbg.set_param("knot","material","BaseWhite")
        dbg.set("knot","position.x",90.0); dbg.set("knot","position.z",-40.0)

        dbg.set("perlin","density",5.0); dbg.set("perlin","displacement",0.3); dbg.set("perlin","timeDensity",4.0)
        dbg.set("wave","amplitude",8.0); dbg.set("wave","frequency",3.0); dbg.set("wave","speed",1.5)
        dbg.set("hue","brightness",1.3); dbg.set("hue","saturation",1.1)
        dbg.set("hue_anim","operation",2.0); dbg.set("hue_anim","b",180.0)

        dbg.set_param("galaxy",  "template", "BBFx/Galaxy")
        dbg.set_param("confetti","template", "BBFx/Confetti")
        dbg.set("galaxy","position.x",-90.0); dbg.set("galaxy","position.z",40.0)
        dbg.set("confetti","position.y",60.0)

        dbg.set_param("backdrop", "mode", "camera_locked")
        dbg.set("backdrop", "alpha", 0.5)
        -- Off by default — toggled on/off live via the gamepad (Lot AT showcase).
        dbg.set("backdrop", "enabled", 0.0)

        dbg.set_param("cycle", "textures", "BumpyMetal.jpg;Chrome.jpg;RustySteel.jpg;NMHollyBumps.png")
        -- period = 60/(bpm*mult) must stay > transition_time so the fade fully resolves.
        dbg.set_param("cycle", "mode", "bpm_synced"); dbg.set_param("cycle","transition_time","1.0"); dbg.set_param("cycle","auto_advance_bpm","0.25")
        dbg.set_param("blend", "tex_a", "BumpyMetal.jpg"); dbg.set_param("blend","tex_b","Chrome.jpg")
        dbg.set_param("blend", "mask", "aureola.png"); dbg.set_param("blend","blend_mode","screen")
        dbg.set("blend","scroll_u_a",0.04); dbg.set("blend","rotate_b",0.02)

        dbg.set_param("router_toggle", "button_index", "0"); dbg.set_param("router_toggle","mode","toggle")

        dbg.set_param("cam", "mode", "orbit")
        dbg.set("cam","orbit_radius",220.0); dbg.set("cam","orbit_speed",0.12); dbg.set("cam","orbit_height",50.0)

        dbg.set("light_key","position.x",100.0); dbg.set("light_key","position.y",120.0); dbg.set("light_key","position.z",100.0)
        dbg.set("light_fill","position.x",-100.0); dbg.set("light_fill","position.y",-30.0); dbg.set("light_fill","position.z",-60.0)
        dbg.set("light_fill","diffuse.r",0.3); dbg.set("light_fill","diffuse.g",0.4); dbg.set("light_fill","diffuse.b",0.8)

        -- Links
        dbg.link("time","dt","perlin","dt"); dbg.link("time","dt","wave","dt"); dbg.link("time","dt","cam","dt")
        dbg.link("time","dt","clip_bg","dt"); dbg.link("time","dt","cycle","dt"); dbg.link("time","dt","beat_trig","dt")
        dbg.link("time","beat","cycle","beat"); dbg.link("time","beat","beat_trig","beat"); dbg.link("time","beatFrac","beat_trig","beatFrac")

        dbg.link("geosphere","entity","perlin","entity")
        dbg.link("knot","entity","wave","entity")
        dbg.link("knot","entity","hue","entity")
        dbg.link("geosphere","entity","hue","entity")

        -- Hue cycle.
        dbg.link("time","beatFrac","hue_anim","a")
        dbg.link("hue_anim","out","hue","hue_shift")

        -- Particles : Galaxy hue from time, Confetti pulsed by BeatTrigger.
        dbg.link("time","beatFrac","galaxy","color.b")
        dbg.link("time","beat","galaxy","emission_rate")
        dbg.link("beat_trig","trigger","confetti","enabled")

        -- Background video → fullscreen overlay (camera-locked).
        dbg.link("clip_bg","material_ready","bridge_bg","material_source")
        dbg.link("backdrop","entity","bridge_bg","entity")
        dbg.link("clip_bg","material_ready","backdrop","material_source")

        -- TextureCycle/Blend → knot (alternate material driven by the cycle).
        dbg.link("blend","material_ready","bridge_knot","material_source")
        dbg.link("knot","entity","bridge_knot","entity")

        -- Gamepad button → toggle the backdrop overlay on/off (Lot AT).
        dbg.link("gamepad","buttonA","router_toggle","button")
        dbg.link("router_toggle","toggled","backdrop","enabled")
    end
}
