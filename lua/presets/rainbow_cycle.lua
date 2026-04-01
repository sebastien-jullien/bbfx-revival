local ParamSpec = require "paramspec"
return {
    name = "rainbow_cycle", version = 2, category = "Color",
    description = "Full spectrum sweep",
    tags = {"rainbow", "spectrum"},
    params = ParamSpec.declare({
        ParamSpec.float("hue_shift", 240.0, {min=0, max=360, label="Hue Shift"}),
        ParamSpec.float("saturation", 1.0, {min=0, max=2, label="Saturation"}),
        ParamSpec.float("brightness", 1.0, {min=0, max=2, label="Brightness"}),
    }),
    build = function(params) return {type="ColorShiftNode", params=params} end
}
