-- ============================================================================
-- BBFx v3.5 plugin template — Preset (installable project snippet)
-- ============================================================================

return {
    onEnable = function(ctx)
        local plugin = ctx and ctx.plugin or bbfx.plugin
        plugin.registerPreset("my-preset", {
            name        = "My Preset",
            description = "A fresh preset ready to use.",
            nodes = {
                { type = "PerlinFxNode", name = "my_perlin" },
            },
            links = {
                -- { src = "..", srcPort = "out", dst = "..", dstPort = "in" }
            },
        })
    end,
}
