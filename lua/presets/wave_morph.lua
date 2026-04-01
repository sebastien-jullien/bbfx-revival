local ParamSpec = require "paramspec"
return {
    name = "wave_morph", version = 2, category = "Geometry",
    description = "Sinusoidal wave deformation on geosphere",
    tags = {"wave", "deformation"},
    params = ParamSpec.declare({
        ParamSpec.float("amplitude", 3.0, {min=0, max=10, label="Amplitude"}),
        ParamSpec.float("frequency", 3.0, {min=0.5, max=20, label="Frequency"}),
        ParamSpec.float("speed", 1.5, {min=0, max=10, label="Speed"}),
        ParamSpec.float("axis", 1.0, {min=0, max=2, label="Axis (0=X 1=Y 2=Z)"}),
    }),
    build = function(params) return {type="WaveVertexShader", params=params} end
}
