-- ============================================================================
-- BBFx v3.5 plugin template — Shader plugin
-- ============================================================================
-- Ships a pair of `.material` and `.frag` files in the plugin's resources
-- directory. plugin.loadShader returns the filename for a ShaderFxNode.

return {
    onEnable = function(ctx)
        local plugin = ctx and ctx.plugin or bbfx.plugin
        -- Expect the user to ship my_shader.frag + my_shader.material
        -- in the plugin folder.
        local frag = plugin.loadShader("my_shader.frag")
        local mat  = plugin.loadMaterial("my_shader.material")
        if frag and mat then
            print("[my-shader] loaded frag=" .. tostring(frag)
                   .. " material=" .. tostring(mat))
        else
            print("[my-shader] resources missing — ship my_shader.frag + "
                   .. "my_shader.material in the plugin dir.")
        end
    end,
}
