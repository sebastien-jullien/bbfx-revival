-- ============================================================================
-- BBFx v3.5 example plugin — L-System Tree
-- ============================================================================
-- Generates a procedural fractal tree from a simple L-system and returns
-- its OGRE mesh name. Uses bbfx.lsystem.* from Lot R.
--
-- Default axiom + rule produce a classic Koch-style branching tree.
-- ============================================================================

local currentMeshName = nil
local currentKey      = nil

local function makeMesh(iterations, angle, length)
    local ls = bbfx.lsystem.create({
        axiom = "F",
        rules = { F = "F[+F]F[-F]F" },
        iterations = math.max(1, math.floor(iterations or 3)),
        angle = angle or 25.7,
        step  = length or 1.0,
    })
    local name = string.format("lsys_tree_%d_%.1f_%.1f",
        iterations or 3, angle or 25.7, length or 1.0)
    ls.generateMesh(name)
    return name
end

return {
    onEnable = function(ctx)
        local plugin = ctx and ctx.plugin or bbfx.plugin
        if plugin and plugin.registerNodeType then
            plugin.registerNodeType("LSystemTree", {
                category = "Scene",
                color    = { 0.3, 0.8, 0.4, 1.0 },
                inputs   = { "iterations", "angle", "length" },
                outputs  = { "meshName" },
                params   = {
                    iterations = { default = 3,    min = 1,   max = 5    },
                    angle      = { default = 25.7, min = 5.0, max = 90.0 },
                    length     = { default = 1.0,  min = 0.1, max = 5.0  },
                },
                process = function(self, ports)
                    local it  = ports.iterations or 3
                    local ang = ports.angle or 25.7
                    local len = ports.length or 1.0
                    local key = string.format("%d:%.2f:%.2f", it, ang, len)
                    if key ~= currentKey then
                        currentMeshName = makeMesh(it, ang, len)
                        currentKey = key
                    end
                    ports.meshName = currentMeshName or ""
                end,
            })
        end
    end,

    onDisable = function()
        currentMeshName = nil
        currentKey      = nil
    end,
}
