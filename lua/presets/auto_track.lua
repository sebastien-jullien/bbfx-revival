local ParamSpec = require "paramspec"
return {
    name = "auto_track", version = 2, category = "Camera",
    description = "Camera that tracks and follows the first scene entity",
    tags = {"camera", "track", "follow", "target"},
    params = ParamSpec.declare({
        ParamSpec.float("damping", 0.1, {min=0.01, max=1, label="Damping"}),
        ParamSpec.float("fov", 45, {min=20, max=90, label="FOV"}),
    }),
    build = function(params)
        params.mode = "track"
        return {type="CameraNode", params=params}
    end
}
