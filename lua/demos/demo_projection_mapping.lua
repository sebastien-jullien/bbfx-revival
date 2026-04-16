-- demo_projection_mapping.lua — BBFx v3.4 "Stage"
-- Demonstrates: 2-zone projection mapping setup.
-- - 2 named zones (left canvas, right canvas)
-- - 2 output windows, each assigned to a zone
-- - Quad warp applied per output to simulate projector alignment
-- - Edge blend on the overlapping border
--
-- Usage:
--   bbfx-studio.exe lua/demos/demo_projection_mapping.lua

print("[demo_projection_mapping] Setting up 2-zone projection mapping...")

-- ── Scene ─────────────────────────────────────────────────────────────────────
dbg.create("PerlinFxNode",   "perlin")
dbg.create("SceneObjectNode","torus")
dbg.create("ColorShiftNode", "color")

dbg.set_param("torus", "mesh_file", "torus.mesh")
dbg.set("torus", "scale.x", 2.5)
dbg.set("torus", "scale.y", 2.5)
dbg.set("torus", "scale.z", 2.5)
dbg.link("perlin", "mesh_dirty", "torus", "rotation.y")
dbg.link("perlin", "mesh_dirty", "color", "hue_shift")

-- ── Surface zones (2 zones side by side) ─────────────────────────────────────
-- Zone 0: left half of the canvas (x=0, y=0, w=0.5, h=1.0)
-- Zone 1: right half of the canvas (x=0.5, y=0, w=0.5, h=1.0)
dbg.zone_add("Left Canvas",  0.0, 0.0, 0.5, 1.0)  -- zone 0
dbg.zone_add("Right Canvas", 0.5, 0.0, 0.5, 1.0)  -- zone 1

-- ── Output windows ────────────────────────────────────────────────────────────
dbg.output_add(1920, 1080)  -- output 0 → left projector
dbg.output_add(1920, 1080)  -- output 1 → right projector

-- ── Assign zones to outputs ───────────────────────────────────────────────────
dbg.zone_assign(0, 0)  -- zone 0 (left) → output 0
dbg.zone_assign(1, 1)  -- zone 1 (right) → output 1

-- ── Warp: simulate slight convergence of two projectors ───────────────────────
-- Left projector: slight right-side keystone (projector angled right)
dbg.output_warp(0,
    0.05, 0.0,   0.95, 0.05,
    0.0,  1.0,   1.0,  1.0)

-- Right projector: slight left-side keystone (projector angled left)
dbg.output_warp(1,
    0.0,  0.05,  0.95, 0.0,
    0.0,  1.0,   1.0,  1.0)

-- ── Edge blend: 15% overlap on the adjoining edges ───────────────────────────
-- Left output: blend right edge
dbg.output_blend(0,  0.0, 0.15, 0.0, 0.0, 2.2)   -- right=0.15, gamma=2.2
-- Right output: blend left edge
dbg.output_blend(1,  0.15, 0.0, 0.0, 0.0, 2.2)   -- left=0.15, gamma=2.2

-- ── Scene Switcher demo (v3.4 Lot P) ────────────────────────────────────────
-- Capture 2 scenes with different warp configurations, then switch between them.

-- Scene "Left Focus": left projector aligned, right projector skewed
dbg.scene_capture("Left Focus")

-- Now tweak warp: shift emphasis to right projector
dbg.output_warp(0,
    0.0,  0.05,  0.95, 0.0,
    0.0,  1.0,   1.0,  1.0)
dbg.output_warp(1,
    0.05, 0.0,   0.95, 0.05,
    0.0,  1.0,   1.0,  1.0)

dbg.scene_capture("Right Focus")

-- Restore original warp for initial state
dbg.output_warp(0,
    0.05, 0.0,   0.95, 0.05,
    0.0,  1.0,   1.0,  1.0)
dbg.output_warp(1,
    0.0,  0.05,  0.95, 0.0,
    0.0,  1.0,   1.0,  1.0)

print("[demo_projection_mapping] Setup complete:")
print("  2 zones: Left Canvas (0-50%) + Right Canvas (50-100%)")
print("  2 outputs with quad warp (simulated projector angles)")
print("  Edge blend: 15% overlap with gamma 2.2")
print("  2 scenes: 'Left Focus' + 'Right Focus'")
print("")
print("  Use 'Stage > Output Manager' to fine-tune warp and blend per output.")
print("  Use 'Stage > Surface Editor' to edit zone boundaries.")
print("  Use dbg.scene_apply('Left Focus') or dbg.scene_apply('Right Focus') to switch scenes.")
print("  Use dbg.scene_list() to see all captured scenes.")
print("  Press Ctrl+Shift+P for PANIC ALL.")
