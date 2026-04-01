-- bonneballe_basic.lua -- BBFx Template: BonneBalle Basic
return {
    name = "BonneBalle Basic",
    bpm = 120,
    description = "Geosphere + Perlin + orbit camera",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end
    end
}
