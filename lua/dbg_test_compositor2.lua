-- Test compositor step by step with screenshots
-- Run: ./bbfx-studio.exe lua/dbg_test_compositor2.lua

print("\n=== Compositor Debug Test v2 ===\n")

-- Create compositor nodes
dbg.create_with_param("CompositorNode", "test_bloom", "compositor", "Bloom")

-- Take RT1 screenshot (scene only)
dbg.screenshot("test_rt1_before.png")

-- Check compositor status
dbg.compositor_status()

-- Show what getCompositedTextureID returns vs getRenderTextureID
print("[TEST] RT1 and RT2 texture IDs will be logged at next frame")
