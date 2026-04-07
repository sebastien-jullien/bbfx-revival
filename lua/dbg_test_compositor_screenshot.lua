-- Test: create compositor, wait, take screenshot
print("=== Compositor Screenshot Test ===")

-- Create compositor
dbg.create_with_param("CompositorNode", "sc_bloom", "compositor", "Bloom")

-- The screenshot captures RT1 which should have the compositor effect
-- But we need to wait for the deferred create + compositor application
-- Let's not switch to F5 — just check if the compositor affects Studio Mode too
print("[TEST] Compositor created. Screenshot in Studio Mode...")
