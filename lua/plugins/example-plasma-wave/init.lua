-- ============================================================================
-- BBFx v3.5 example plugin — Plasma Wave
-- ============================================================================
-- Audio-reactive plasma procedural noise driven by FFT + beat + manual gain.
-- Registers a single node type "PlasmaWave" with ports :
--   * speed     (float, 0..10)     — wave travel speed
--   * scale     (float, 0.01..1)   — spatial frequency
--   * audioGain (float, 0..4)      — FFT low-band influence
--   * time      (float, input)     — driven by bbfx.tempo.getBeat()
-- Outputs :
--   * value  (float) — raw plasma value, useful for MixerNode fade
--   * r,g,b  (float) — cheap RGB from the same value
-- ============================================================================

return {
    onEnable = function(ctx)
        local plugin = ctx and ctx.plugin or bbfx.plugin
        if plugin and plugin.registerNodeType then
            plugin.registerNodeType("PlasmaWave", {
                category = "Source",
                color    = { 0.5, 0.9, 1.0, 1.0 },
                inputs   = { "speed", "scale", "audioGain", "time" },
                outputs  = { "value", "r", "g", "b" },
                params   = {
                    speed     = { default = 1.0,  min = 0.0, max = 10.0 },
                    scale     = { default = 0.2,  min = 0.01, max = 1.0 },
                    audioGain = { default = 1.0,  min = 0.0, max = 4.0 },
                },
                process = function(self, ports)
                    local t   = ports.time  or 0.0
                    local s   = ports.speed or 1.0
                    local sc  = ports.scale or 0.2
                    local ag  = ports.audioGain or 1.0
                    -- Audio : use FFT low-band if available.
                    local audio = 0.0
                    if bbfx.audio and bbfx.audio.getBands then
                        local b = bbfx.audio.getBands()
                        if b and b[1] then audio = b[1] * ag end
                    end
                    local phase = t * s + audio
                    local v = math.sin(phase * sc * 2.0 * math.pi)
                    ports.value = v
                    ports.r = 0.5 + 0.5 * math.sin(phase + 0.0)
                    ports.g = 0.5 + 0.5 * math.sin(phase + 2.094)
                    ports.b = 0.5 + 0.5 * math.sin(phase + 4.188)
                end,
            })
        end
    end,

    onDisable = function() end,
}
