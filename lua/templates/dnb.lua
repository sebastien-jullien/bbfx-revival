-- dnb.lua -- BBFx Template: Drum & Bass
return {
    name = "Drum & Bass",
    bpm = 172,
    description = "Fast nervous visuals for D&B",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(172) end
    end
}
