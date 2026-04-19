-- ============================================================================
-- BBFx v3.5 plugin template — ImGui Panel plugin
-- ============================================================================
-- Registers a dockable ImGui panel. Requires the `ui` permission.

local state = { sliderVal = 0.5, counter = 0 }

return {
    onEnable = function(ctx)
        if not bbfx.ui then
            print("[my-panel] bbfx.ui unavailable — add 'ui' permission")
            return
        end
        bbfx.ui.registerPanel("My Plugin Panel", function()
            bbfx.ui.text("Hello from my plugin!")
            local changed, v = bbfx.ui.sliderFloat("Value", state.sliderVal, 0, 1)
            if changed then state.sliderVal = v end
            if bbfx.ui.button("Click me!") then
                state.counter = state.counter + 1
            end
            bbfx.ui.text(string.format("Counter: %d", state.counter))
        end)
    end,

    onDisable = function()
        if bbfx.ui then bbfx.ui.unregisterPanel("My Plugin Panel") end
    end,
}
