-- bake_demos.lua — BBFx v3.5.2 Sprint S8 Lot AU
--
-- Runs each demo builder and saves the resulting graph as a .bbfx-project.
-- The builder Lua is the SOURCE OF TRUTH; the .bbfx-project is generated,
-- conformant, regenerable. Re-bake any time you tweak a builder or the
-- schema evolves.
--
-- Usage (from build/windows-debug/Debug/):
--   ./bbfx-studio.exe lua/demos/projects/bake_demos.lua
--
-- This script is loaded as the studio's "scene setup script" — i.e. it runs
-- DURING startup, BEFORE the default project template would be loaded. So the
-- DAG holds only the RootTimeNode (`time`) at this point — a clean slate. We
-- build each demo synchronously, save it, wipe the graph (Animator:removeNode
-- is immediate, unlike the deferred dbg.clear()), repeat, then os.exit(0)
-- before the studio would load anything else.
--
-- Authors: Sebastien Jullien — 2026-05-12

local DEMOS = {
    "demo_studio_base",
    "demo_mesh_morph",
    "demo_particle_garden",
    "demo_texture_set",
    "demo_video_wall",
    "demo_audio_reactive",
    "demo_shader_lab",
    "demo_vj_full",
    "demo_projection_mapping",
    "demo_anim_joystick",
}

print("[bake_demos] BBFx v3.5.2 Demo Showcase Pack — baking " .. #DEMOS .. " demos")

local anim = bbfx.Animator.instance()

local function clearAll()
    if not anim then return end
    -- Animator:removeNode takes an AnimationNode*, not a name → resolve via
    -- getNodeByName first. removeNode is synchronous (unlike the deferred
    -- dbg.clear()/dbg.delete()), which is exactly what we need inside this
    -- startup-time script.
    for _, n in ipairs(anim:getNodeNames()) do
        if n ~= "time" then
            local node = anim:getNodeByName(n)
            if node then pcall(function() anim:removeNode(node) end) end
        end
    end
    if _dbg_process_pending then _dbg_process_pending() end
end

local okCount, failCount = 0, 0

local function bakeOne(name)
    local builderPath = "lua/demos/projects/" .. name .. "_builder.lua"
    local outPath     = "lua/demos/projects/" .. name .. ".bbfx-project"
    print("[bake_demos] " .. name)

    clearAll()

    local ok, builder = pcall(dofile, builderPath)
    if not ok then
        print("  ! dofile failed: " .. tostring(builder)); failCount = failCount + 1; return
    end
    if type(builder) ~= "table" or type(builder.setup) ~= "function" then
        print("  ! builder did not return { setup = function }"); failCount = failCount + 1; return
    end
    local sok, serr = pcall(builder.setup)
    if not sok then
        print("  ! setup() error: " .. tostring(serr)); failCount = failCount + 1; return
    end
    if _dbg_process_pending then _dbg_process_pending() end

    local n = anim and #anim:getNodeNames() or 0
    local saved = dbg.save(outPath)
    if saved then
        print(string.format("  OK (%d nodes) -> %s", n, outPath)); okCount = okCount + 1
    else
        print("  ! dbg.save failed for " .. outPath); failCount = failCount + 1
    end
end

for _, name in ipairs(DEMOS) do bakeOne(name) end
clearAll()

print(string.format("[bake_demos] Done. %d OK / %d FAIL out of %d.", okCount, failCount, #DEMOS))
os.exit(failCount == 0 and 0 or 1)
