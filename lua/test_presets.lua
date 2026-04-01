-- test_presets.lua — Automated preset testing script
-- Launches Studio, tests each preset category, takes screenshots

-- Wait for Studio to be ready
local function wait(n)
    local t = os.clock() + (n or 0.5)
    while os.clock() < t do end
end

print("=== BBFx Preset Test Suite ===")

-- First verify perlin_pulse (golden reference)
print("\n--- TEST: perlin_pulse (reference) ---")
dbg.clear()
dbg.preset("perlin_pulse")
