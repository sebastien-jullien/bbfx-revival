-- ============================================================================
-- BBFx Test Suite Runner
-- ============================================================================
-- Executes test_suite.lua with synchronous deferred operation processing.
-- W() calls _dbg_process_pending() directly to flush the deferred queue.
-- Usage: cd build/windows-debug/Debug && ./bbfx-studio.exe lua/tests/run_tests.lua
-- ============================================================================

print("[test_runner] Test runner starting...")

-- NOTE: Do NOT define W or WAIT_FRAMES here.
-- coroutine.yield() cannot cross the sol2 C++ → Lua boundary.
-- test_suite.lua defines its own W() that calls _dbg_process_pending() directly.

local testStarted = false
local frameCount = 0

local function tick()
    frameCount = frameCount + 1

    -- Wait for initialization (10 frames)
    if frameCount < 10 then return end

    -- Run test suite once
    if not testStarted then
        testStarted = true
        print("[test_runner] Executing test suite...\n")

        -- Load and execute test_suite.lua directly (no coroutine needed)
        local testFunc, loadErr = loadfile("lua/tests/test_suite.lua")
        if not testFunc then
            print("[test_runner] Failed to load test_suite.lua: " .. tostring(loadErr))
            return
        end
        local ok, err = pcall(testFunc)
        if not ok then
            print("\n[test_runner] TEST SUITE ERROR: " .. tostring(err))
        end
        print("\n[test_runner] Test suite complete.")
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
