local ParamSpec = require "paramspec"
return {
    name = "audio_reactive_sphere", version = 2, category = "Composition",
    description = "Iconic BonneBalle: bass drives displacement, mids shift color, highs boost bloom",
    tags = {"audio", "reactive", "sphere", "iconic", "composition"},
    params = ParamSpec.declare({
        ParamSpec.mesh("mesh", "geosphere4500.mesh", {label="Mesh"}),
        ParamSpec.float("displacement", 0.15, {min=0, max=20, label="Displacement"}),
        ParamSpec.float("density", 4.0, {min=0.1, max=10, label="Noise Scale"}),
        ParamSpec.float("timeDensity", 5.0, {min=0.1, max=20, label="Time Density"}),
        ParamSpec.float("audio_sensitivity", 1.0, {min=0.1, max=5, label="Audio Sensitivity"}),
        ParamSpec.float("bass_response", 2.0, {min=0, max=10, label="Bass -> Displacement"}),
        ParamSpec.float("mid_response", 60, {min=0, max=360, label="Mid -> Hue Speed"}),
        ParamSpec.float("high_response", 1.5, {min=0, max=5, label="High -> Bloom"}),
        ParamSpec.bool("camera_orbit", true, {label="Camera Orbit"}),
        ParamSpec.float("orbit_speed", 0.3, {min=0, max=2, label="Orbit Speed"}),
    }),
    build = function(params)
        return {
            type = "PerlinFxNode",
            primary = "fx",
            nodes = {
                {name="mesh", type="SceneObjectNode", params={mesh_file="geosphere4500.mesh"}},
                {name="fx",   type="PerlinFxNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="fx", toPort="entity"},
            },
            params = params,
        }
    end
}
