-- hiphop.lua -- BBFx Template: Hip-Hop
return {
    name = "Hip-Hop",
    bpm = 90,
    description = "Cool flowing visuals for hip-hop",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(90) end
    end
}
