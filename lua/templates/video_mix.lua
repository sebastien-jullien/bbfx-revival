-- video_mix.lua -- BBFx Template: Video Mix
return {
    name = "Video Mix",
    bpm = 120,
    description = "2 video sources with crossfade",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end
    end
}
