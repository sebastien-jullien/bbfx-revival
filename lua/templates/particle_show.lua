-- particle_show.lua -- BBFx Template: Particle Show
return {
    name = "Particle Show",
    bpm = 140,
    description = "4 particle systems with triggers",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(140) end
    end
}
