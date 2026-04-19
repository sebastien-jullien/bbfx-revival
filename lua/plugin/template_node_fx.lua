-- ============================================================================
-- BBFx v3.5 plugin template — FX Node (transforms an input to an output)
-- ============================================================================

return {
    onEnable = function(ctx)
        local plugin = ctx and ctx.plugin or bbfx.plugin
        plugin.registerNodeType("my-fx", {
            category = "Fx",
            color    = { 1.0, 0.6, 0.3, 1.0 },
            inputs   = { "in", "gain" },
            outputs  = { "out" },
            process  = function(self, ports)
                local x = ports["in"] or 0.0
                local g = ports.gain or 1.0
                ports.out = x * g
            end,
        })
    end,
}
