-- techno.lua -- BBFx Template: Techno
return {
    name = "Techno",
    bpm = 135,
    description = "Hard geometric visuals for techno",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(135) end
    end
}
