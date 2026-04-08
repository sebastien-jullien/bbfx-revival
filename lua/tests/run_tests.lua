-- ============================================================================
-- BBFx Test Suite Runner (multi-frame)
-- ============================================================================
-- Executes test_suite.lua across multiple frames to allow deferred operations
-- (node creation/deletion) to complete between test groups.
-- Usage: cd build/windows-debug/Debug && ./bbfx-studio.exe lua/tests/run_tests.lua
-- ============================================================================

print("[test_runner] Multi-frame test runner starting...")

-- We override W() to actually count real frames via the tick callback
local pendingFrames = 0
local testCoroutine = nil
local testStarted = false
local frameCount = 0

-- Global wait function that yields the coroutine for N real frames
function WAIT_FRAMES(n)
    pendingFrames = n or 1
    coroutine.yield()
end

-- Replace W in the test suite (1 frame default = fast, 2+ for deferred ops)
function W(n)
    WAIT_FRAMES(n or 1)
end

local function tick()
    frameCount = frameCount + 1

    -- Wait for initialization (10 frames)
    if frameCount < 10 then return end

    -- Start test coroutine on first ready frame
    if not testStarted then
        testStarted = true
        testCoroutine = coroutine.create(function()
            print("[test_runner] Executing test suite (multi-frame)...\n")
            local ok, err = pcall(function()
                dofile("lua/tests/test_suite.lua")
            end)
            if not ok then
                print("\n[test_runner] TEST SUITE ERROR: " .. tostring(err))
            end
            print("\n[test_runner] Test suite complete.")
        end)
        -- Initial resume to start the coroutine (runs until first W/yield)
        local ok, err = coroutine.resume(testCoroutine)
        if not ok then print("[test_runner] Initial resume error: " .. tostring(err)) end
        return  -- don't double-resume this frame
    end

    -- Pending debugger ops (create/delete/load) are now processed in StudioApp main loop
    -- (between frames, before DAG evaluation) — no need to call here

    -- Resume coroutine if waiting frames are done
    if testCoroutine and coroutine.status(testCoroutine) == "suspended" then
        if pendingFrames > 0 then
            pendingFrames = pendingFrames - 1
        else
            local ok, err = coroutine.resume(testCoroutine)
            if not ok then
                print("[test_runner] Coroutine error: " .. tostring(err))
                testCoroutine = nil
            end
        end
    end
end

-- Register frame callback
local animator = bbfx.Animator.instance()
local testNode = bbfx.LuaAnimationNode("_test_runner", function(self)
    tick()
end)
testNode:addInput("dt")
animator:addNode(testNode)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", testNode, "dt")
end
