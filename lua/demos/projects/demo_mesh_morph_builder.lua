-- demo_mesh_morph_builder.lua — BBFx v3.5.2 Sprint S8 Lot AU (I-2005)
--
-- Showcase : meshes 3D + déformateurs vertex + caméra mobile.
--   - geosphere déformée par PerlinFxNode (bruit animé)
--   - knot déformé par WaveVertexShader (onde sinus) + ColorShift hue cycle
--   - athene en rotation lente (lien direct time.beat → rotation.y)
--   - CameraNode en mode `orbit` (la vue tourne autour de la scène)
--
-- Layout : caméra MOBILE (orbite) · pas de texture/vidéo caméra · 110 BPM.

return {
    name = "Mesh & Deformers",
    bpm = 110,
    description = "Geosphere déformée Perlin + knot ondulé par WaveVertexShader (hue cycle) + athene rotatif — caméra en orbite.",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(110) end

        dbg.create("SceneObjectNode",  "geosphere")
        dbg.create("SceneObjectNode",  "knot")
        dbg.create("SceneObjectNode",  "athene")
        dbg.create("LightNode",        "light_key")
        dbg.create("LightNode",        "light_fill")
        dbg.create("CameraNode",       "cam_orbit")
        dbg.create("PerlinFxNode",     "perlin")
        dbg.create("WaveVertexShader", "wave_knot")
        dbg.create("ColorShiftNode",   "hue_knot")
        dbg.create("MathNode",         "hue_anim")
        _dbg_process_pending()

        dbg.set_param("geosphere", "mesh_file", "geosphere4500.mesh")
        dbg.set_param("geosphere", "material",  "BaseWhiteNoLighting")
        dbg.set_param("knot",      "mesh_file", "knot.mesh")
        dbg.set_param("knot",      "material",  "Examples/Chrome")
        dbg.set_param("athene",    "mesh_file", "athene.mesh")
        dbg.set_param("athene",    "material",  "BaseWhite")

        dbg.set("geosphere", "position.x",  0.0)
        dbg.set("geosphere", "position.y", 25.0)
        dbg.set("geosphere", "scale.x", 0.05); dbg.set("geosphere","scale.y",0.05); dbg.set("geosphere","scale.z",0.05)
        dbg.set("knot",   "position.x",  70.0); dbg.set("knot","position.z",-50.0)
        dbg.set("athene", "position.x", -70.0); dbg.set("athene","position.z",-50.0)
        dbg.set("athene", "scale.x", 0.5); dbg.set("athene","scale.y",0.5); dbg.set("athene","scale.z",0.5)

        dbg.set("perlin", "density", 6.0)
        dbg.set("perlin", "displacement", 0.35)
        dbg.set("perlin", "timeDensity", 4.0)

        dbg.set("wave_knot", "amplitude", 8.0)
        dbg.set("wave_knot", "frequency", 3.0)
        dbg.set("wave_knot", "speed", 1.5)
        dbg.set("wave_knot", "axis", 1.0)

        dbg.set("hue_knot", "brightness", 1.2)
        dbg.set("hue_knot", "saturation", 1.0)
        dbg.set("hue_anim", "operation", 2.0)   -- multiply
        dbg.set("hue_anim", "b", 180.0)         -- ±180° hue sweep

        -- CameraNode in orbit mode : the view circles the scene.
        dbg.set_param("cam_orbit", "mode", "orbit")
        dbg.set("cam_orbit", "orbit_radius", 200.0)
        dbg.set("cam_orbit", "orbit_speed",   0.15)
        dbg.set("cam_orbit", "orbit_height",  40.0)

        dbg.set("light_key",  "position.x", 100.0); dbg.set("light_key","position.y",120.0); dbg.set("light_key","position.z",100.0)
        dbg.set("light_fill", "position.x",-100.0); dbg.set("light_fill","position.y",-30.0); dbg.set("light_fill","position.z",-60.0)
        dbg.set("light_fill", "diffuse.r", 0.3); dbg.set("light_fill","diffuse.g",0.4); dbg.set("light_fill","diffuse.b",0.8)

        -- Links
        dbg.link("time", "dt", "perlin",    "dt")
        dbg.link("time", "dt", "wave_knot", "dt")
        dbg.link("time", "dt", "cam_orbit", "dt")
        dbg.link("geosphere", "entity", "perlin",    "entity")
        dbg.link("knot",      "entity", "wave_knot", "entity")
        dbg.link("knot",      "entity", "hue_knot",  "entity")

        dbg.link("time", "beatFrac", "hue_anim", "a")
        dbg.link("hue_anim", "out",  "hue_knot", "hue_shift")

        dbg.link("time", "beat", "knot",   "rotation.y")
        dbg.link("time", "beat", "athene", "rotation.y")
    end
}
