-- ============================================================================
-- BBFx v3.5 plugin template — Generator Node (produces a value over time)
-- ============================================================================
-- Edit the metadata + process() body, then `bbfx.plugin.reload(<id>)` to
-- test your changes live.

return {
    onEnable = function(ctx)
        local plugin = ctx and ctx.plugin or bbfx.plugin
        plugin.registerNodeType("my-generator", {
            category = "Source",
            color    = { 0.4, 0.8, 1.0, 1.0 },
            inputs   = { "time" },
            outputs  = { "value" },
            process  = function(self, ports)
                local t = ports.time or 0.0
                ports.value = math.sin(t * 2.0 * math.pi)
            end,
        })
        print("[my-generator] registered")
    end,

    onDisable = function()
        print("[my-generator] disabled")
    end,
}
