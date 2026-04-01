-- shader_lab.lua -- BBFx Template: Shader Lab
return {
    name = "Shader Lab",
    bpm = 120,
    description = "Explore GPU shaders",
    setup = function()
        -- Template scene setup
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end
    end
}
