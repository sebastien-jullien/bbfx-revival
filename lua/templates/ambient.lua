-- ambient.lua -- BBFx Template: Ambient
return {
    name = "Ambient",
    bpm = 70,
    description = "Slow organic visuals for ambient music",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(70) end
    end
}
