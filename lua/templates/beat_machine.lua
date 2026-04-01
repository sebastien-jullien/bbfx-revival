-- beat_machine.lua -- BBFx Template: Beat Machine
return {
    name = "Beat Machine",
    bpm = 128,
    description = "4 presets on chord triggers",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(128) end
    end
}
