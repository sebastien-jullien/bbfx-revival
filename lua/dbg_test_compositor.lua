-- dbg_test_compositor.lua — Automated compositor test script
-- Run: ./bbfx-studio.exe lua/dbg_test_compositor.lua
-- Or from console: dofile("lua/dbg_test_compositor.lua")

print("\n=== BBFx Compositor Test ===\n")

-- Step 1: List existing nodes
print("[TEST] Current nodes:")
dbg.list()

-- Step 2: Create a CompositorNode with "Bloom" compositor
print("\n[TEST] Creating CompositorNode 'test_bloom'...")
dbg.create_with_param("CompositorNode", "test_bloom", "compositor", "Bloom")

-- Wait for deferred creation
-- (dbg.create is deferred, needs _dbg_process_pending to execute)

-- Step 3: Create another with "B&W"
print("[TEST] Creating CompositorNode 'test_bw'...")
dbg.create_with_param("CompositorNode", "test_bw", "compositor", "B&W")

-- Step 4: Check compositor status after a short delay
-- Note: The nodes are created deferred, so we need to check after processing
print("\n[TEST] Compositor status (may be empty until next frame):")
dbg.compositor_status()

-- Step 5: List nodes to verify creation
print("\n[TEST] Nodes after compositor creation:")
dbg.list()

-- Step 6: Trace each compositor node
print("\n[TEST] Tracing test_bloom:")
dbg.trace("test_bloom")
print("\n[TEST] Tracing test_bw:")
dbg.trace("test_bw")

-- Step 7: Check links
print("\n[TEST] Links:")
dbg.links()

-- Step 8: Switch to Performance Mode
print("\n[TEST] Switching to Performance Mode...")
dbg.mode("performance")

-- Step 9: Take screenshot after mode switch
print("[TEST] Taking screenshot (will be captured on next frame)...")
-- Note: screenshot is immediate but the mode switch is via SDL event
-- so we need the main loop to process it first

print("\n=== Compositor Test Complete ===")
print("Check console output for [CompositorStack] Applied messages")
print("Take screenshot manually after F5 is active: dbg.screenshot('compositor_test.png')")
