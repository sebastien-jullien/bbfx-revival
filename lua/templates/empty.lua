-- empty.lua -- BBFx Template: Empty Project
return {
    name = "Empty Project",
    bpm = 120,
    description = "Start from scratch",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end
    end
}
