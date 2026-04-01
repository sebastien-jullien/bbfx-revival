-- house.lua -- BBFx Template: House
return {
    name = "House",
    bpm = 124,
    description = "Warm pulsing visuals for house music",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(124) end
    end
}
