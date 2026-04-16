-- demo_network_sync.lua — BBFx v3.4 "Stage"
-- Demonstrates: network sync between multiple BBFx instances (master/slave).
--
-- Usage:
--   Master: bbfx-studio.exe lua/demos/demo_network_sync.lua master
--   Slave:  bbfx-studio.exe lua/demos/demo_network_sync.lua slave
--
-- Or run without argument for standalone (no sync):
--   bbfx-studio.exe lua/demos/demo_network_sync.lua

local arg = arg  -- command-line args table (may be nil in studio mode)
local role = (arg and arg[1]) or "standalone"

print("[demo_network_sync] Role: " .. role)

-- ── Create a simple scene ────────────────────────────────────────────────────
dbg.create("PerlinFxNode", "perlin")
dbg.create("SceneObjectNode", "sphere")
dbg.set_param("sphere", "mesh_file", "sphere.mesh")
dbg.link("perlin", "mesh_dirty", "sphere", "rotation.y")

-- ── Start network sync ───────────────────────────────────────────────────────
if role == "master" then
    dbg.sync_start("master")
    print("[demo_network_sync] Started as MASTER.")
    print("  Start a slave instance on another machine in the same network.")
    print("  The master will auto-discover slaves via UDP broadcast.")
    print("")
    print("  To send a chord:  dbg.sync_chord('chord_name')")
    print("  To push beat:     dbg.sync_beat(120.0, 1)")
    print("  To panic slaves:  dbg.sync_panic()")

elseif role == "slave" then
    dbg.sync_start("slave")
    print("[demo_network_sync] Started as SLAVE.")
    print("  Looking for master on the network (UDP broadcast port 33196)...")
    print("  Once connected, chord transitions from the master will be applied here.")

else
    print("[demo_network_sync] Standalone mode — no network sync.")
    print("  Run with argument 'master' or 'slave' to enable sync.")
end

-- ── Log sync events (will be visible in Console panel) ───────────────────────
print("[demo_network_sync] Scene ready. Open 'Stage > Network Sync' panel to monitor peers.")
