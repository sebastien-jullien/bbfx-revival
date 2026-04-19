-- ============================================================================
-- BBFx v3.5 Lot N — Noise / Easing / Tempo / Timeline Lua API
-- ============================================================================
-- Coverage : I-1424..I-1437
--   - bbfx.noise.*   : simplex 2D/3D/4D + worley 2D/3D + curl 2D/3D + fbm +
--                     generateTexture (signature-level test)
--   - bbfx.easing    : loaded from lua/plugin/easing.lua, 30+ functions
--   - bbfx.tempo     : setSource/setManualBPM/getBPM/onNext{Beat,Bar}/subdivision
--   - bbfx.timeline  : create/addKey/addEvent/play/update/getValue
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot N — Noise / Easing / Tempo / Timeline")
print("================================================================\n")

local P, F = 0, 0
local fails = {}
local function check(id, name, cond, extra)
    if cond then
        P = P + 1; print("  PASS  " .. id .. " " .. name)
    else
        F = F + 1
        print("  FAIL  " .. id .. " " .. name .. (extra and ("  [" .. tostring(extra) .. "]") or ""))
        table.insert(fails, id .. " " .. name)
    end
end

-- ── Namespaces exist ─────────────────────────────────────────────────────
for _, ns in ipairs({"noise","easing","tempo","timeline"}) do
    check("N-001." .. ns, "bbfx." .. ns .. " namespace exists",
          type(bbfx[ns]) == "table")
end

-- ── Noise signatures (I-1424..I-1426) ────────────────────────────────────
for _, fn in ipairs({"simplex2D","simplex3D","simplex4D","worley2D","worley3D",
                     "curl2D","curl3D","fbm2D","generateTexture"}) do
    check("N-002." .. fn, "bbfx.noise." .. fn .. " is a function",
          type(bbfx.noise[fn]) == "function")
end

-- Simplex determinism: same seed -> same value
local s1 = bbfx.noise.simplex2D(1.23, 4.56, 42)
local s2 = bbfx.noise.simplex2D(1.23, 4.56, 42)
check("N-003", "simplex2D deterministic with same seed",
      math.abs(s1 - s2) < 1e-6)

-- Simplex range is roughly [-1, 1]
local minV, maxV = 1e9, -1e9
for i = 0, 99 do
    for j = 0, 99 do
        local v = bbfx.noise.simplex2D(i * 0.1, j * 0.1, 7)
        if v < minV then minV = v end
        if v > maxV then maxV = v end
    end
end
check("N-004", "simplex2D samples cover a meaningful range",
      minV < -0.3 and maxV > 0.3 and minV >= -1.1 and maxV <= 1.1)

-- Worley distance is in [0, 1]
local w = bbfx.noise.worley2D(0.5, 0.5, 3)
check("N-005", "worley2D returns value in [0, 1]",
      w >= 0 and w <= 1)

-- Curl 2D returns two components
local cx, cy = bbfx.noise.curl2D(0.3, 0.4, 1)
check("N-006", "curl2D returns tuple (x, y)",
      type(cx) == "number" and type(cy) == "number")

-- fbm default options
local fv = bbfx.noise.fbm2D(0.1, 0.2, { octaves = 4, seed = 0 })
check("N-007", "fbm2D returns a number",
      type(fv) == "number")

-- ── Easing library (I-1427..I-1430) ──────────────────────────────────────
check("N-010", "easing.linear(0.5) == 0.5",
      math.abs(bbfx.easing.linear(0.5) - 0.5) < 1e-6)
check("N-011", "easing.easeInOutCubic(0.5) == 0.5",
      math.abs(bbfx.easing.easeInOutCubic(0.5) - 0.5) < 1e-6)
check("N-012", "easeInQuad(0) == 0 and easeInQuad(1) == 1",
      math.abs(bbfx.easing.easeInQuad(0)) < 1e-6 and
      math.abs(bbfx.easing.easeInQuad(1) - 1) < 1e-6)
check("N-013", "easeInOutSine(0) == 0 and (1) == 1",
      math.abs(bbfx.easing.easeInOutSine(0)) < 1e-6 and
      math.abs(bbfx.easing.easeInOutSine(1) - 1) < 1e-4)

-- lerp helpers
local l = bbfx.easing.lerp(0, 10, 0.5, "linear")
check("N-014", "easing.lerp(0, 10, 0.5) == 5", math.abs(l - 5) < 1e-6)
local r, g, b = bbfx.easing.lerpColor(0, 0, 0, 1, 1, 1, 0.5, "linear")
check("N-015", "lerpColor midpoint = (0.5, 0.5, 0.5)",
      math.abs(r - 0.5) < 1e-6 and math.abs(g - 0.5) < 1e-6 and math.abs(b - 0.5) < 1e-6)

local b0 = bbfx.easing.bezier(0,   0, 0.2, 0.8, 1)
local b1 = bbfx.easing.bezier(1,   0, 0.2, 0.8, 1)
local bm = bbfx.easing.bezier(0.5, 0, 0.2, 0.8, 1)
check("N-016", "bezier endpoints match (0 -> 0, 1 -> 1)",
      math.abs(b0) < 1e-6 and math.abs(b1 - 1) < 1e-6)
check("N-017", "bezier midpoint strictly between endpoints",
      bm > b0 and bm < b1)

check("N-018", "easing.names returns a list",
      type(bbfx.easing.names()) == "table" and #bbfx.easing.names() > 20)

-- ── Tempo (I-1431..I-1433) ────────────────────────────────────────────────
for _, fn in ipairs({"getBPM","getBeat","getBeatPhase","setManualBPM",
                     "setSource","getSource","onNextBeat","onNextBar",
                     "onSubdivision","off"}) do
    check("N-020." .. fn, "bbfx.tempo." .. fn .. " is a function",
          type(bbfx.tempo[fn]) == "function")
end
bbfx.tempo.setSource("manual")
bbfx.tempo.setManualBPM(125.0)
check("N-021", "tempo source is manual",
      bbfx.tempo.getSource() == "manual")
check("N-022", "manual BPM round-trips",
      math.abs(bbfx.tempo.getBPM() - 125.0) < 0.01)

-- Switch source (should not crash)
bbfx.tempo.setSource("audio")
check("N-023", "tempo source audio selectable",
      bbfx.tempo.getSource() == "audio")
bbfx.tempo.setSource("midi_clock")
check("N-024", "tempo source midi_clock selectable",
      bbfx.tempo.getSource() == "midi_clock")
bbfx.tempo.setSource("manual")  -- restore

-- ── Timeline (I-1434..I-1436) ────────────────────────────────────────────
check("N-030", "bbfx.timeline.create is a function",
      type(bbfx.timeline.create) == "function")

local tl = bbfx.timeline.create({ duration = 10.0, loop = false })
check("N-031", "timeline.create returns a table",
      type(tl) == "table")
for _, m in ipairs({"addKey","addEvent","play","pause","stop","seek","setSpeed",
                    "setLoop","getValue","getCurrentTime","getDuration",
                    "isPlaying","update","clearKeys","clearEvents"}) do
    check("N-032." .. m, "timeline." .. m .. " is a function",
          type(tl[m]) == "function")
end

-- 3 keys: 0 -> 0, 5 -> 50, 10 -> 100 (linear interp by default).
tl.addKey(0,   0,   "linear")
tl.addKey(5,   50,  "linear")
tl.addKey(10,  100, "linear")

tl.seek(0)
check("N-033", "getValue at t=0 is 0",
      math.abs(tl.getValue()) < 1e-4)
tl.seek(5)
check("N-034", "getValue at t=5 is 50",
      math.abs(tl.getValue() - 50) < 1e-4)
tl.seek(7.5)
check("N-035", "getValue at t=7.5 is 75 (interp)",
      math.abs(tl.getValue() - 75) < 1e-4)

-- Event firing — play 30 seconds in one big step (loop off so it will
-- clamp to duration). Event at t=5 should fire exactly once.
local fired = 0
tl.addEvent(5.0, function() fired = fired + 1 end)
tl.seek(0)
tl.play()
tl.update(30.0)
check("N-036", "event at t=5 fired when play head crosses it",
      fired == 1)
check("N-037", "timeline is stopped after exceeding duration (loop off)",
      tl.isPlaying() == false and
      math.abs(tl.getCurrentTime() - tl.getDuration()) < 1e-4)

-- ── Non-regression : Lot M/L bindings still reachable ───────────────────
check("N-099.a", "bbfx.midi still reachable",
      type(bbfx.midi.getCC) == "function")
check("N-099.b", "bbfx.osc still reachable",
      type(bbfx.osc.send) == "function")
check("N-099.c", "bbfx.gamepad still reachable",
      type(bbfx.gamepad.count) == "function")

print("\n--------------------------------------------------------------")
print(string.format("  Lot N Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot N tests FAILED")
end

os.exit(0)
