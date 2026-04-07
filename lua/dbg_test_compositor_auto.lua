-- Test Studio: F5 sans compositor (pas de traînées) + F5 avec Bloom
print("=== Studio Compositor Regression Test ===")
-- No compositor created — F5 should show the ogre normally without trails
dbg.mode("performance")
print("[TEST] F5 without compositor. Should show ogre cleanly, no trails.")
