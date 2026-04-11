-- test_trigger_cleanup.lua — Verify trigger preset activate/deactivate
print("\n=== Trigger Cleanup Test ===\n")

if not W then
    function W(n) for i = 1, (n or 1) do _dbg_process_pending() end end
end

local baseCount = #dbg.list()
print("Baseline: " .. baseCount .. " nodes")

-- Test 1: snowfall (single-node ParticleNode)
print("\n--- Test 1: snowfall ---")
dbg.preset("snowfall")
W(5)
print("After activate: " .. #dbg.list() .. " nodes")
dbg.delete("snowfall")
W(5)
local c1 = #dbg.list()
print("After delete: " .. c1 .. " nodes")
print(c1 == baseCount and "  PASS" or "  FAIL (leftover)")

-- Test 2: perlin_pulse (multi-node)
print("\n--- Test 2: perlin_pulse ---")
dbg.preset("perlin_pulse")
W(5)
print("After activate: " .. #dbg.list() .. " nodes")
for _, n in ipairs(dbg.list()) do print("  node: " .. n) end
dbg.delete("perlin_pulse_fx")
W(5)
local c2 = #dbg.list()
print("After delete: " .. c2 .. " nodes")
print(c2 == baseCount and "  PASS" or "  FAIL (leftover)")

-- Test 3: monochrome_fade (single-node ColorShiftNode)
print("\n--- Test 3: monochrome_fade ---")
dbg.preset("monochrome_fade")
W(5)
print("After activate: " .. #dbg.list() .. " nodes")
dbg.delete("monochrome_fade")
W(5)
local c3 = #dbg.list()
print("After delete: " .. c3 .. " nodes")
print(c3 == baseCount and "  PASS" or "  FAIL (leftover)")

print("\n=== DONE ===")
