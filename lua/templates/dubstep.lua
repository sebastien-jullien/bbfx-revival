-- dubstep.lua -- BBFx Template: Dubstep
return {
    name = "Dubstep",
    bpm = 140,
    description = "Heavy bass drop visuals for dubstep",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(140) end
    end
}
