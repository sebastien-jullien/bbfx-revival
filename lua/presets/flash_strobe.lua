local ParamSpec = require "paramspec"
return {
    name = "flash_strobe", version = 2, category = "Color",
    description = "White flash on each beat onset - classic VJ effect",
    tags = {"flash", "strobe", "beat", "vj", "light"},
    params = ParamSpec.declare({
        ParamSpec.float("power", 8.0, {min=0, max=20, label="Light Power"}),
        ParamSpec.float("diffuse.r", 1.0, {min=0, max=1, label="Diffuse R"}),
        ParamSpec.float("diffuse.g", 1.0, {min=0, max=1, label="Diffuse G"}),
        ParamSpec.float("diffuse.b", 0.9, {min=0, max=1, label="Diffuse B"}),
    }),
    build = function(params) return {type="LightNode", params=params} end
}
