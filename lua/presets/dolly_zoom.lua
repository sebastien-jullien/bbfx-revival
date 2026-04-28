local ParamSpec = require "paramspec"
return {
    name = "dolly_zoom", version = 2, category = "Camera",
    description = "Hitchcock Vertigo effect — dolly zoom with inverse FOV",
    tags = {"camera", "dolly", "vertigo", "hitchcock"},
    params = ParamSpec.declare({
        ParamSpec.float("dolly_speed", 1.5, {min=0.1, max=5, label="Speed"}),
        ParamSpec.float("target_distance", 15, {min=3, max=50, label="Target Dist"}),
    }),
    build = function(params)
        params.mode = "dolly_zoom"
        return {type="CameraNode", params=params}
    end
}
