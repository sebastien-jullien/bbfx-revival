local ParamSpec = require "paramspec"
return {
    name = "material_cycle", version = 2, category = "Color",
    description = "Saturated hue cycling with boosted color intensity",
    tags = {"color", "hue", "cycle", "saturated"},
    params = ParamSpec.declare({
        ParamSpec.float("hue_shift", 0.0, {min=0, max=360, label="Hue Shift"}),
        ParamSpec.float("saturation", 1.2, {min=0, max=2, label="Saturation"}),
        ParamSpec.float("brightness", 1.0, {min=0, max=2, label="Brightness"}),
    }),
    build = function(params) return {type="ColorShiftNode", params=params} end
}
