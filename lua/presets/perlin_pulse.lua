-- perlin_pulse — Pulsating Perlin noise synced to beats
local ParamSpec = require 'paramspec'
return {
    name = "perlin_pulse",
    version = 2,
    category = "Geometry",
    description = "Geosphere with Perlin displacement pulsing to the beat",
    tags = {"perlin", "beat", "sphere", "deformation"},
    params = ParamSpec.declare({
        ParamSpec.mesh("mesh", "geosphere4500.mesh", {label="Mesh"}),
        ParamSpec.float("displacement", 0.15, {min=0, max=20, label="Displacement"}),
        ParamSpec.float("density", 4.0, {min=0.1, max=10, label="Noise Scale"}),
        ParamSpec.float("timeDensity", 5.0, {min=0, max=20, label="Speed"}),
        ParamSpec.bool("beat_sync", true, {label="Sync to Beat"}),
    }),
    build = function(params)
        return {
            type = "PerlinFxNode",
            primary = "fx",
            nodes = {
                {name="mesh", type="SceneObjectNode", params={mesh_file="ogrehead.mesh"}},
                {name="fx",   type="PerlinFxNode"},
            },
            links = {
                {from="mesh", fromPort="entity", to="fx", toPort="entity"},
            },
            params = params,
        }
    end
}
