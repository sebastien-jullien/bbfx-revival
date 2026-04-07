-- Test RT2 rendering WITHOUT compositors
print("\n=== RT2 No-Compositor Test ===\n")

-- No compositor nodes. Switch to F5 and take screenshot.
dbg.mode("performance")

-- Screenshot will be taken on next frames via the deferred mechanism
-- But we need the mode switch to happen first (it's via SDL event)
-- The screenshot must happen AFTER the SDL event is processed

-- Use a timer approach: register a one-shot callback
-- For now, just take a screenshot immediately (it will capture the NEXT frame)
dbg.screenshot("test_rt2_nocomp_f5.png")

print("[TEST] Screenshot saved as test_rt2_nocomp_f5.png")
print("[TEST] Check if it shows the scene or grey")
