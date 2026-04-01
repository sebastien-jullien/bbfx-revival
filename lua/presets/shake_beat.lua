local ParamSpec = require "paramspec"
return {
    name = "shake_beat", version = 2, category = "Camera",
    description = "Camera shake on audio onset",
    tags = {"shake", "beat"},
    params = ParamSpec.declare({
        ParamSpec.float("orbit_radius", 130, {min=5, max=200, label="Radius"}),
        ParamSpec.float("orbit_speed", 1.0, {min=0.05, max=5, label="Speed"}),
        ParamSpec.float("orbit_height", 20, {min=-50, max=50, label="Height"}),
        ParamSpec.float("fov", 50, {min=10, max=120, label="FOV"}),
    }),
    build = function(params) return {type="CameraNode", params=params} end
}
