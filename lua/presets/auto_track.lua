local ParamSpec = require "paramspec"
return {
    name = "auto_track", version = 2, category = "Camera",
    description = "Auto-follow active object",
    tags = {"track", "follow"},
    params = ParamSpec.declare({
        ParamSpec.float("orbit_radius", 120, {min=5, max=200, label="Radius"}),
        ParamSpec.float("orbit_speed", 0.3, {min=0.05, max=5, label="Speed"}),
        ParamSpec.float("orbit_height", 15, {min=-50, max=50, label="Height"}),
        ParamSpec.float("fov", 45, {min=10, max=120, label="FOV"}),
    }),
    build = function(params) return {type="CameraNode", params=params} end
}
