local ParamSpec = require "paramspec"
return {
    name = "dolly_zoom", version = 2, category = "Camera",
    description = "Hitchcock vertigo zoom",
    tags = {"dolly", "vertigo"},
    params = ParamSpec.declare({
        ParamSpec.float("orbit_radius", 160, {min=5, max=200, label="Radius"}),
        ParamSpec.float("orbit_speed", 0.2, {min=0.05, max=5, label="Speed"}),
        ParamSpec.float("orbit_height", 15, {min=-50, max=50, label="Height"}),
        ParamSpec.float("fov", 70, {min=10, max=120, label="FOV"}),
    }),
    build = function(params) return {type="CameraNode", params=params} end
}
