-- perlin_pulse.lua — BBFx Preset
-- A pulsating perlin noise effect driven by beat detection

return {
    name = "perlin_pulse",
    description = "Pulsating perlin noise synced to beats",
    nodes = {
        { type = "PerlinFxNode", name = "perlin", params = { displacement = 2.0 } },
    }
}
