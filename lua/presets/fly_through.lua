local ParamSpec = require "paramspec"
return {
    name = "fly_through", version = 2, category = "Camera",
    description = "Camera flying through scene",
    tags = {"fly", "tunnel"},
    params = ParamSpec.declare({
        ParamSpec.float("orbit_radius", 200, {min=5, max=200, label="Radius"}),
        ParamSpec.float("orbit_speed", 0.5, {min=0.05, max=5, label="Speed"}),
        ParamSpec.float("orbit_height", 40, {min=-50, max=50, label="Height"}),
        ParamSpec.float("fov", 55, {min=10, max=120, label="FOV"}),
    }),
    build = function(params) return {type="CameraNode", params=params} end
}
