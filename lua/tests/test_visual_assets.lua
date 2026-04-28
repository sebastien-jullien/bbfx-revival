-- ============================================================================
-- BBFx Visual Asset Tester
-- ============================================================================
-- Creates each asset one by one, waits for rendering, takes screenshots.
-- Uses frame-based state machine via LuaAnimationNode callback.
-- Usage: cd build/windows-debug/Debug && ./bbfx-studio.exe lua/tests/test_visual_assets.lua
-- Screenshots saved to: screenshots/particle_*.png, screenshots/mesh_*.png
-- ============================================================================

print("[visual] Visual asset tester starting...")

local frameCount = 0
local testIndex = 0
local phaseFrame = 0
local testDone = false

local INIT_FRAMES  = 30   -- wait for app/OGRE init
local WAIT_RENDER  = 90   -- frames to let particles build up (~1.5s at 60fps)
local WAIT_MESH    = 30   -- frames for mesh rendering (instant, just need 1 render)

local function sanitize(s)
    return s:gsub("[/%s%.]+", "_")
end

-- ======================== TEST QUEUE ========================

local tests = {}

-- Particle templates
local particleTemplates = {
    "BBFx/Fire", "BBFx/ElectricArc", "BBFx/Confetti",
    "BBFx/MagicDust", "BBFx/NeonTrail", "BBFx/Bubbles",
    "BBFx/LaserBeam", "BBFx/Galaxy", "BBFx/MatrixRain",
    "BBFx/ParticleTunnel", "BBFx/StarField", "BBFx/SparkBurst",
    "BBFx/Snowfall",
    "Examples/Smoke", "Examples/GreenyNimbus",
    "Examples/Rain", "Examples/Snow", "Examples/Aureola",
    "Examples/Swarm", "Examples/Fountain1", "Examples/Fountain2",
    "Examples/Fireworks", "Examples/PurpleFountain",
}
for _, t in ipairs(particleTemplates) do
    table.insert(tests, {
        category   = "particle",
        nodeType   = "ParticleNode",
        nodeName   = "vtest_" .. sanitize(t),
        paramName  = "template",
        paramValue = t,
        label      = t,
        screenshot = "screenshots/particle_" .. sanitize(t) .. ".png",
        waitFrames = WAIT_RENDER,
    })
end

-- Mesh assets
local meshFiles = {
    "geosphere4500.mesh", "geosphere8000.mesh", "sphere.mesh",
    "cube.mesh", "knot.mesh", "column.mesh",
    "athene.mesh", "ogrehead.mesh", "ninja.mesh",
    "robot.mesh", "fish.mesh", "facial.mesh",
    "razor.mesh", "RZR-002.mesh",
    "Barrel.mesh", "WoodPallet.mesh",
}
for _, m in ipairs(meshFiles) do
    table.insert(tests, {
        category   = "mesh",
        nodeType   = "SceneObjectNode",
        nodeName   = "vtest_" .. sanitize(m),
        paramName  = "mesh_file",
        paramValue = m,
        label      = m,
        screenshot = "screenshots/mesh_" .. sanitize(m) .. ".png",
        waitFrames = WAIT_MESH,
    })
end

local results = {}

-- ======================== STATE MACHINE ========================

local function tick()
    frameCount = frameCount + 1
    if frameCount < INIT_FRAMES then return end
    if testDone then return end

    -- Start first test
    if testIndex == 0 then
        testIndex = 1
        phaseFrame = 0
        print(string.format("[visual] Starting visual tests: %d assets to test", #tests))
        return
    end

    -- All tests done
    if testIndex > #tests then
        testDone = true
        print("\n[visual] ========================================")
        print("[visual]           VISUAL TEST RESULTS")
        print("[visual] ========================================")
        local okCount = 0
        local currentCat = ""
        for _, r in ipairs(results) do
            if r.category ~= currentCat then
                currentCat = r.category
                print(string.format("\n  --- %s ---", currentCat:upper()))
            end
            if r.ok then okCount = okCount + 1 end
            print(string.format("  [%s] %-30s → %s",
                r.ok and "OK" or "!!", r.label, r.screenshot))
        end
        print(string.format("\n[visual] Screenshots: %d/%d saved", okCount, #results))
        print("[visual] DONE — inspect screenshots in build/windows-debug/Debug/screenshots/")
        return
    end

    phaseFrame = phaseFrame + 1
    local test = tests[testIndex]

    if phaseFrame == 1 then
        -- Create node with param
        print(string.format("[visual] [%d/%d] %s: %s",
            testIndex, #tests, test.category, test.label))
        dbg.create_with_param(test.nodeType, test.nodeName, test.paramName, test.paramValue)

    elseif phaseFrame == 3 then
        -- Process deferred creation
        if _dbg_process_pending then _dbg_process_pending() end

    elseif phaseFrame == 5 then
        -- Second flush (safety)
        if _dbg_process_pending then _dbg_process_pending() end

    elseif phaseFrame == test.waitFrames then
        -- Take screenshot
        local ok = dbg.screenshot(test.screenshot)
        print(string.format("[visual]   → %s (%s)",
            test.screenshot, ok and "saved" or "FAILED"))
        table.insert(results, {
            category   = test.category,
            label      = test.label,
            screenshot = test.screenshot,
            ok         = ok,
        })

    elseif phaseFrame == test.waitFrames + 3 then
        -- Delete node
        dbg.delete(test.nodeName)

    elseif phaseFrame == test.waitFrames + 5 then
        -- Flush delete
        if _dbg_process_pending then _dbg_process_pending() end
        if _dbg_flush_deletes then _dbg_flush_deletes() end

    elseif phaseFrame == test.waitFrames + 8 then
        -- Final flush + advance
        if _dbg_process_pending then _dbg_process_pending() end
        if _dbg_flush_deletes then _dbg_flush_deletes() end
        testIndex = testIndex + 1
        phaseFrame = 0
    end
end

-- ======================== SETUP ========================

local animator = bbfx.Animator.instance()
local testNode = bbfx.LuaAnimationNode("_visual_tester", function(self)
    local ok, err = pcall(tick)
    if not ok then
        print("[visual] ERROR: " .. tostring(err))
        testDone = true
    end
end)
testNode:addInput("dt")
animator:addNode(testNode)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", testNode, "dt")
    print("[visual] Tester initialized — waiting " .. INIT_FRAMES .. " frames for startup...")
else
    print("[visual] ERROR: root_time not found!")
end
