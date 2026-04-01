-- full_performance.lua -- BBFx Template: Full Performance
return {
    name = "Full Performance",
    bpm = 128,
    description = "Complete VJ set pre-configured",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(128) end
    end
}
