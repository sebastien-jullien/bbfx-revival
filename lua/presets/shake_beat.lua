local ParamSpec = require "paramspec"
return {
    name = "shake_beat", version = 2, category = "Camera",
    description = "Orbit camera with beat-driven shake intensity",
    tags = {"camera", "shake", "beat", "audio"},
    params = ParamSpec.declare({
        ParamSpec.float("shake_intensity", 0.5, {min=0, max=1, label="Shake"}),
        ParamSpec.float("orbit_radius", 25, {min=5, max=100, label="Radius"}),
        ParamSpec.float("orbit_speed", 0.3, {min=0.05, max=2, label="Speed"}),
    }),
    build = function(params)
        params.mode = "shake"
        return {type="CameraNode", params=params}
    end
}
