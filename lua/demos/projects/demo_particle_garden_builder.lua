-- demo_particle_garden_builder.lua — BBFx v3.5.2 Sprint S8 Lot AU (I-2006)
--
-- Showcase : 6 systèmes de particules de la bibliothèque BBFx répartis en
-- hexagone, couleurs/émission animées par time + BeatDetector (micro), Confetti
-- pulsé on/off par un BeatTrigger (Lot AT). Caméra en mode fly_through.
--
-- Layout : caméra MOBILE (fly_through) · pas de texture/vidéo caméra · 128 BPM.

return {
    name = "Particle Garden",
    bpm = 128,
    description = "6 systèmes (Fire/Galaxy/Snowfall/Confetti/MagicDust/NeonTrail) en hexagone, beat-réactifs, caméra fly-through.",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(128) end

        dbg.create("ParticleNode",     "fire")
        dbg.create("ParticleNode",     "galaxy")
        dbg.create("ParticleNode",     "snowfall")
        dbg.create("ParticleNode",     "confetti")
        dbg.create("ParticleNode",     "magicdust")
        dbg.create("ParticleNode",     "neontrail")
        dbg.create("LightNode",        "light")
        dbg.create("CameraNode",       "cam")
        dbg.create("BeatDetectorNode", "beat")
        dbg.create("BeatTriggerNode",  "burst")
        _dbg_process_pending()

        dbg.set_param("fire",      "template", "BBFx/Fire")
        dbg.set_param("galaxy",    "template", "BBFx/Galaxy")
        dbg.set_param("snowfall",  "template", "BBFx/Snowfall")
        dbg.set_param("confetti",  "template", "BBFx/Confetti")
        dbg.set_param("magicdust", "template", "BBFx/MagicDust")
        dbg.set_param("neontrail", "template", "BBFx/NeonTrail")

        local pos = {
            fire      = {  80,   0,    0 },
            galaxy    = {  40,  40,  -70 },
            snowfall  = { -40,  60,  -70 },
            confetti  = { -80,   0,    0 },
            magicdust = { -40, -10,   70 },
            neontrail = {  40,  20,   70 },
        }
        for name, p in pairs(pos) do
            dbg.set(name, "position.x", p[1]); dbg.set(name,"position.y",p[2]); dbg.set(name,"position.z",p[3])
        end

        dbg.set_param("cam", "mode", "fly_through")
        dbg.set("cam", "fly_speed", 12.0)

        dbg.set("light", "position.x", 0.0); dbg.set("light","position.y",200.0); dbg.set("light","position.z",0.0)
        dbg.set("light", "diffuse.r", 1.0); dbg.set("light","diffuse.g",0.9); dbg.set("light","diffuse.b",0.75)

        -- Links
        dbg.link("time", "dt", "cam",  "dt")
        dbg.link("time", "dt", "beat", "dt")
        dbg.link("time", "dt", "burst","dt")
        dbg.link("time", "beat",     "burst", "beat")
        dbg.link("time", "beatFrac", "burst", "beatFrac")

        -- Micro beat → emission boost on Fire & Galaxy.
        dbg.link("beat", "beat", "fire",   "emission_rate")
        dbg.link("beat", "beat", "galaxy", "emission_rate")
        -- Time-based ambient emission on the others.
        dbg.link("time", "beatFrac", "snowfall",  "emission_rate")
        dbg.link("time", "beatFrac", "magicdust", "emission_rate")
        dbg.link("time", "beatFrac", "neontrail", "emission_rate")

        -- BeatTrigger pulses Confetti on/off at each beat (Lot AT).
        dbg.link("burst", "trigger", "confetti", "enabled")

        -- Hue cycling on the particles from time channels.
        dbg.link("time", "beatFrac", "fire",      "color.r")
        dbg.link("time", "beat",     "fire",      "color.g")
        dbg.link("time", "beatFrac", "galaxy",    "color.b")
        dbg.link("time", "beatFrac", "magicdust", "color.r")
        dbg.link("time", "beat",     "neontrail", "color.g")
        dbg.link("time", "beat",     "snowfall",  "color.b")
    end
}
