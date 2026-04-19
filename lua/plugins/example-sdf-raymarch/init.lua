-- ============================================================================
-- BBFx v3.5 example plugin — SDF Raymarch
-- ============================================================================
-- A post-process node that renders a scene of 3 SDF primitives (sphere,
-- box, torus) blended with Inigo-Quilez smoothUnion. Uses the built-in
-- bbfx.sdf.* API from Lot R.
--
-- Ports :
--   * shape    (int 0..2)   — 0 sphere / 1 box / 2 torus
--   * distance (float)      — camera distance
--   * softness (float 0..1) — smoothUnion blend factor
-- Output :
--   * meshName (string)     — generated OGRE mesh usable by SceneObjectNode
-- ============================================================================

local currentMeshName = nil

local function buildField(shape, softness)
    return function(x, y, z)
        if shape == 1 then
            return bbfx.sdf.box(x, y, z, 0, 0, 0, 0.5, 0.5, 0.5)
        elseif shape == 2 then
            return bbfx.sdf.torus(x, y, z, 0, 0, 0, 0.7, 0.2)
        else
            local s1 = bbfx.sdf.sphere(x, y, z, -0.4, 0, 0, 0.5)
            local s2 = bbfx.sdf.sphere(x, y, z,  0.4, 0, 0, 0.5)
            return bbfx.sdf.opSmoothUnion(s1, s2, softness)
        end
    end
end

return {
    onEnable = function(ctx)
        local plugin = ctx and ctx.plugin or bbfx.plugin
        if plugin and plugin.registerNodeType then
            plugin.registerNodeType("SDFRaymarch", {
                category = "PostProcess",
                color    = { 0.7, 0.5, 1.0, 1.0 },
                inputs   = { "shape", "distance", "softness" },
                outputs  = { "meshName" },
                params   = {
                    shape    = { default = 0,   min = 0,    max = 2   },
                    distance = { default = 4.0, min = 1.0,  max = 10  },
                    softness = { default = 0.3, min = 0.01, max = 1.0 },
                },
                process = function(self, ports)
                    local shape = math.floor(ports.shape or 0)
                    local soft  = ports.softness or 0.3
                    local name  = "sdf_raymarch_" .. tostring(shape)
                    if currentMeshName ~= name then
                        local f = buildField(shape, soft)
                        local r = bbfx.sdf.toMesh(name, f,
                            -1.2, -1.2, -1.2, 1.2, 1.2, 1.2, 20)
                        if r and #r > 0 then currentMeshName = r end
                    end
                    ports.meshName = currentMeshName or ""
                end,
            })
        end
    end,

    onDisable = function()
        currentMeshName = nil
    end,
}
