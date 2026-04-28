local ParamSpec = require "paramspec"
return {
    name = "fly_through", version = 2, category = "Camera",
    description = "Linear fly-through camera along Z axis with ping-pong",
    tags = {"camera", "fly", "linear", "motion"},
    params = ParamSpec.declare({
        ParamSpec.float("fly_speed", 3, {min=0.5, max=20, label="Speed"}),
        ParamSpec.float("fov", 60, {min=20, max=120, label="FOV"}),
    }),
    build = function(params)
        params.mode = "fly_through"
        return {type="CameraNode", params=params}
    end
}
