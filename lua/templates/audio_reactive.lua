-- audio_reactive.lua -- BBFx Template: Audio Reactive
return {
    name = "Audio Reactive",
    bpm = 0,
    description = "Microphone input drives visuals",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(0) end
    end
}
