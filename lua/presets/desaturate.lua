local ParamSpec = require "paramspec"
return {
    name = "desaturate", version = 2, category = "Color",
    description = "Progressive desaturation from full color to monochrome",
    tags = {"color", "bw", "desaturate", "monochrome"},
    params = ParamSpec.declare({
        ParamSpec.float("hue_shift", 0.0, {min=0, max=360, label="Hue Shift"}),
        ParamSpec.float("saturation", 0.0, {min=0, max=2, label="Saturation"}),
        ParamSpec.float("brightness", 0.8, {min=0, max=2, label="Brightness"}),
    }),
    build = function(params) return {type="ColorShiftNode", params=params} end
}
