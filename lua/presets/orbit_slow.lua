local ParamSpec = require "paramspec"
return {
    name = "orbit_slow", version = 2, category = "Camera",
    description = "Slow contemplative orbital camera around the subject",
    tags = {"orbit", "slow", "contemplative", "camera"},
    params = ParamSpec.declare({
        ParamSpec.float("orbit_radius", 150, {min=5, max=500, label="Radius"}),
        ParamSpec.float("orbit_speed", 0.15, {min=0.05, max=2, label="Speed"}),
        ParamSpec.float("orbit_height", 25, {min=-50, max=100, label="Height"}),
        ParamSpec.float("fov", 45, {min=20, max=90, label="FOV"}),
    }),
    build = function(params) return {type="CameraNode", params=params} end
}
