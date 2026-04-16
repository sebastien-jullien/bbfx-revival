-- demo_multi_output.lua — BBFx v3.4 "Stage"
-- Demonstrates: 3 simultaneous output windows, each with a different quad warp preset.
-- Scene: animated cube with Perlin + audio-reactive shaders.
--
-- Usage:
--   bbfx-studio.exe lua/demos/demo_multi_output.lua
--
-- After launch, open "Stage > Output Manager" (Ctrl+Shift+O) to manage the outputs.

local dbg = dbg  -- debug/command interface

-- ── Create scene ────────────────────────────────────────────────────────────
dbg.create("PerlinFxNode", "perlin")
dbg.create("SceneObjectNode", "cube")
dbg.create("ColorShiftNode", "color")

dbg.set_param("cube", "mesh_file", "ogrehead.mesh")
dbg.set("cube", "scale.x", 2.0)
dbg.set("cube", "scale.y", 2.0)
dbg.set("cube", "scale.z", 2.0)
dbg.set("perlin", "density", 0.8)
dbg.set("perlin", "displacement", 0.4)

dbg.link("perlin", "mesh_dirty", "cube", "rotation.y")
dbg.link("perlin", "mesh_dirty", "color", "hue_shift")

-- Optional: audio-reactive if analyzer is present
if dbg.list and type(dbg.list) == "function" then
    local nodes = dbg.list()
    for _, n in ipairs(nodes) do
        if n:find("AudioAnalyzerNode") then
            dbg.create("ArtnetOutputNode", "dmx1")
            dbg.artnet_quick_assign("dmx1")
            print("[demo_multi_output] Audio analyzer found → DMX auto-linked")
            break
        end
    end
end

-- ── Create 3 output windows ─────────────────────────────────────────────────
-- (These commands call OutputManager, which creates real SDL windows.)
-- In automated test mode, windows are hidden; in studio mode they appear.
print("[demo_multi_output] Creating 3 output windows...")
dbg.output_add(1280, 720)  -- output 0
dbg.output_add(1280, 720)  -- output 1
dbg.output_add(1280, 720)  -- output 2

-- ── Apply different quad warp on each output ─────────────────────────────────
-- Output 0: slight keystone (top wider)
dbg.output_warp(0,
    0.1, 0.0,   -- TL shifted right
    0.9, 0.0,   -- TR shifted left
    0.0, 1.0,   -- BL identity
    1.0, 1.0)   -- BR identity

-- Output 1: mild pincushion (center push)
dbg.output_warp(1,
    0.0, 0.05,  0.95, 0.05,
    0.0, 0.95,  0.95, 0.95)

-- Output 2: identity (straight)
dbg.output_warp_reset(2)

print("[demo_multi_output] Setup complete.")
print("  Use 'Stage > Output Manager' to adjust each output's warp and blend.")
print("  Press Ctrl+Shift+P for PANIC ALL (reset all warps).")
