-- demo_studio_base_builder.lua — BBFx v3.5.2 Sprint S8 Lot AU (I-2015)
--
-- Re-bake nettoyée de la sauvegarde de référence (project.bbfx-project,
-- 2026-04-28). Contenu équivalent + 2 ajouts mineurs (choix user) :
--   - l'oscillator (dead-end dans la référence) → relié à ColorShift.hue_shift
--   - le port `enabled` de Snowfall piloté par un BeatTrigger → vitrine Lot AT
--
-- Le nœud interne `shell/1` du serveur TCP-shell N'EST PAS répliqué (root-cause
-- du cruft fixée dans I-2003). Les 2 LuaAnimationNodes de la référence
-- (rotate_head, oscillator) sont remplacés par des nœuds natifs : MathNode pour
-- l'oscillateur, lien direct time.beat→studio_head.rotation.y pour la rotation.
-- Fonctionnellement équivalent, plus idiomatique DAG, round-trip propre.
--
-- Layout : ogrehead + geosphere (Perlin + 3 textures) · caméra orbitale par
--          défaut · Snowfall + Examples/Swarm · 120 BPM.

return {
    name = "BBFx Studio Base (cleaned)",
    bpm = 120,
    description = "Re-bake nettoyée de la référence — sans shell/1, oscillator branché sur ColorShift, Snowfall pulsée par BeatTrigger (Lot AT). Stamp v3.5.2.",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end

        dbg.create("SceneObjectNode", "studio_head")
        dbg.create("SceneObjectNode", "geosphere")
        dbg.create("LightNode",       "studio_light")
        dbg.create("ColorShiftNode",  "color")
        dbg.create("PerlinFxNode",    "perlin")
        dbg.create("TextureNode",     "tex_rockwall")
        dbg.create("TextureNode",     "tex_nm_bumps")
        dbg.create("TextureNode",     "tex_heat_noise")
        dbg.create("ParticleNode",    "snowfall")
        dbg.create("ParticleNode",    "swarm")
        dbg.create("MathNode",        "osc_sin")
        dbg.create("MathNode",        "osc_scale")
        dbg.create("BeatTriggerNode", "beat_enable")
        _dbg_process_pending()

        dbg.set_param("studio_head", "mesh_file", "ogrehead.mesh")
        dbg.set_param("studio_head", "material",  "BaseWhiteNoLighting")
        dbg.set_param("geosphere",   "mesh_file", "geosphere4500.mesh")
        dbg.set_param("geosphere",   "material",  "BaseWhiteNoLighting")
        dbg.set_param("tex_rockwall",   "texture", "rockwall_NH.tga")
        dbg.set_param("tex_rockwall",   "lighting_mode", "emissive")
        dbg.set_param("tex_nm_bumps",   "texture", "NMBumpsOut.png")
        dbg.set_param("tex_nm_bumps",   "lighting_mode", "lit")
        dbg.set_param("tex_heat_noise", "texture", "HeatNoise.tga")
        dbg.set_param("tex_heat_noise", "lighting_mode", "lit")
        dbg.set_param("snowfall", "template", "BBFx/Snowfall")
        dbg.set_param("swarm",    "template", "Examples/Swarm")

        dbg.set("studio_head", "position.y", 20.0)
        dbg.set("geosphere",   "position.y", -10.0)
        dbg.set("geosphere",   "scale.x", 0.05)
        dbg.set("geosphere",   "scale.y", 0.05)
        dbg.set("geosphere",   "scale.z", 0.05)

        dbg.set("color", "brightness", 2.343)
        dbg.set("color", "hue_shift", -10.0)
        dbg.set("color", "saturation", 0.552)
        dbg.set("perlin", "density", 4.0)
        dbg.set("perlin", "displacement", 0.15)
        dbg.set("perlin", "timeDensity", 5.0)

        dbg.set_param("studio_light", "light_type", "point")
        dbg.set("studio_light", "position.x", 50.0)
        dbg.set("studio_light", "position.y", 80.0)
        dbg.set("studio_light", "position.z", 120.0)

        -- MathNode: osc_sin = sin(time.beat) ; osc_scale = osc_sin * 60 (±60° hue)
        dbg.set("osc_sin",   "operation", 12.0)   -- sin
        dbg.set("osc_scale", "operation",  2.0)   -- multiply
        dbg.set("osc_scale", "b",         60.0)

        -- Links
        dbg.link("time", "dt",   "perlin",     "dt")
        dbg.link("time", "dt",   "beat_enable", "dt")
        dbg.link("time", "beat", "beat_enable", "beat")
        dbg.link("time", "beatFrac", "beat_enable", "beatFrac")
        dbg.link("time", "beat", "osc_sin",   "a")
        dbg.link("osc_sin",   "out", "osc_scale", "a")
        dbg.link("osc_scale", "out", "color",     "hue_shift")  -- ajout user

        dbg.link("time", "beat",     "snowfall", "color.a")
        dbg.link("time", "beatFrac", "snowfall", "color.b")
        dbg.link("time", "total",    "snowfall", "color.g")
        dbg.link("time", "dt",       "snowfall", "color.r")
        dbg.link("time", "beat",     "swarm",    "color.a")
        dbg.link("time", "beatFrac", "swarm",    "emission_rate")
        dbg.link("time", "total",    "swarm",    "color.g")

        dbg.link("time", "beat", "studio_head", "rotation.y")  -- replaces rotate_head LuaAnim

        dbg.link("studio_head", "entity", "color",          "entity")
        dbg.link("geosphere",   "entity", "perlin",         "entity")
        dbg.link("geosphere",   "entity", "tex_rockwall",   "entity")
        dbg.link("geosphere",   "entity", "tex_nm_bumps",   "entity")
        dbg.link("geosphere",   "entity", "tex_heat_noise", "entity")

        -- Lot AT showcase : BeatTrigger pulses Snowfall on/off at each beat.
        dbg.link("beat_enable", "trigger", "snowfall", "enabled")
    end
}
