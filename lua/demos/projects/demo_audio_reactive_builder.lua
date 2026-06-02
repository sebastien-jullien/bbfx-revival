-- demo_audio_reactive_builder.lua — BBFx v3.5.2 Sprint S8 Lot AU (I-2009)
--
-- Showcase : la killer feature SpectrogramTextureNode + réactivité audio.
--   - AudioCaptureNode (micro) → SpectrogramTextureNode (texture du spectre)
--   - MaterialBridgeNode route la texture spectre sur une geosphere déformée Perlin
--   - BeatDetectorNode → ParticleNode (SparkBurst) émission boostée au beat
--   - ColorShiftNode sur la geosphere, hue piloté par le beat
--
-- Layout : caméra FIXE (focus sur la réaction) · texture procédurale sur mèche ·
--          0 BPM (tout est piloté par l'audio).

return {
    name = "Audio Reactive (Spectrogram)",
    bpm = 90,
    description = "Micro → SpectrogramTexture → geosphere Perlin + BeatDetector → SparkBurst + ColorShift hue. Caméra fixe.",
    setup = function()
        -- NB : bpm > 0 — un bpm de 0 fige l'horloge globale (plus de beat/dt) au
        -- chargement du .bbfx-project. La réactivité audio vient du BeatDetector
        -- temps réel + du Spectrogram, pas du bpm.
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(90) end

        dbg.create("AudioCaptureNode",  "mic")
        dbg.spectrogram("spectro", "viridis")        -- helper : creates SpectrogramTextureNode
        dbg.create("SceneObjectNode",   "geosphere")
        dbg.material_bridge("bridge", "", "emissive")
        dbg.create("PerlinFxNode",      "perlin")
        dbg.create("ColorShiftNode",    "hue")
        dbg.create("BeatDetectorNode",  "beat")
        dbg.create("ParticleNode",      "sparks")
        dbg.create("LightNode",         "light")
        _dbg_process_pending()

        dbg.set_param("geosphere", "mesh_file", "geosphere4500.mesh")
        dbg.set_param("geosphere", "material",  "BaseWhiteNoLighting")
        dbg.set("geosphere", "scale.x", 0.07); dbg.set("geosphere","scale.y",0.07); dbg.set("geosphere","scale.z",0.07)

        dbg.set_param("spectro", "frequency_scale", "log")
        dbg.set_param("spectro", "intensity_scale", "db")

        dbg.set_param("sparks", "template", "BBFx/SparkBurst")
        dbg.set("sparks", "position.y", 0.0)

        dbg.set("perlin", "density", 5.0)
        dbg.set("perlin", "displacement", 0.25)
        dbg.set("perlin", "timeDensity", 3.0)

        dbg.set("hue", "brightness", 1.4)
        dbg.set("hue", "saturation", 1.2)

        dbg.set("light", "position.y", 120.0)
        dbg.set("light", "diffuse.r", 0.7); dbg.set("light","diffuse.g",0.8); dbg.set("light","diffuse.b",1.0)

        -- Links
        dbg.link("time", "dt", "perlin", "dt")
        dbg.link("time", "dt", "beat",   "dt")

        -- Audio → spectrogram texture.
        dbg.link("mic", "samples_ready", "spectro", "audio")
        -- Spectrogram texture → geosphere (via bridge, pulls texture_out mirror).
        dbg.link("spectro",   "texture_ready", "bridge", "material_source")
        dbg.link("geosphere", "entity",        "bridge", "entity")
        dbg.link("geosphere", "entity",        "perlin", "entity")
        dbg.link("geosphere", "entity",        "hue",    "entity")

        -- Beat → spark emission boost + hue shift.
        dbg.link("beat", "beat", "sparks", "emission_rate")
        dbg.link("beat", "bpm",  "hue",    "hue_shift")
    end
}
