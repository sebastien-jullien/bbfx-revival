-- ============================================================================
-- BBFx v3.5 Lot Q — Media / Images / Sequences / Models
-- ============================================================================
-- Coverage : I-1460..I-1475
--   * bbfx.media.*     : openVideo + ffmpeg detection + clip transport API
--   * bbfx.images.*    : load() wrapping OGRE TextureManager
--   * bbfx.sequences.* : loadGif + loadSequence with play/update/getTextureName
--   * bbfx.models.*    : import() wrapping Assimp (falls back if missing)
-- We test the binding surface + inert-handle contracts; actual media file
-- playback is integration-level and not exercised in headless CI.
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot Q — Media / Images / Sequences / Models")
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

-- ── Namespaces ───────────────────────────────────────────────────────────
for _, ns in ipairs({"media","images","sequences","models"}) do
    check("Q-001." .. ns, "bbfx." .. ns .. " namespace exists",
          type(bbfx[ns]) == "table")
end

-- ── Media (I-1461..I-1464) ────────────────────────────────────────────────
for _, fn in ipairs({"ffmpegAvailable","openVideo"}) do
    check("Q-002." .. fn, "bbfx.media." .. fn .. " is a function",
          type(bbfx.media[fn]) == "function")
end

check("Q-003", "ffmpegAvailable returns a boolean",
      type(bbfx.media.ffmpegAvailable()) == "boolean")

-- Opening a non-existent file returns a handle with ok=false; the
-- ffmpeg child process exits and `isOpen()` is false. The handle still
-- provides every transport method (they are no-ops).
local clip = bbfx.media.openVideo("/does/not/exist.mp4", { width=320, height=240, fps=24 })
check("Q-004", "openVideo(missing) returns a handle table",
      type(clip) == "table")
for _, m in ipairs({"play","pause","stop","seek","setSpeed","setLoop",
                    "update","close","getTextureName","isOpen",
                    "isPlaying","getWidth","getHeight","getFPS"}) do
    check("Q-005." .. m, "clip." .. m .. " is a function",
          type(clip[m]) == "function")
end
-- Transport methods must not throw on an inert clip.
local okClose = pcall(function() clip.pause(); clip.stop(); clip.close() end)
check("Q-006", "clip transport methods are callable on inert handle",
      okClose == true)

-- ── Images (I-1465..I-1466) ───────────────────────────────────────────────
check("Q-010", "bbfx.images.load is a function",
      type(bbfx.images.load) == "function")
-- Loading a missing file returns nil (not a handle).
local imgBad = bbfx.images.load("/does/not/exist.png")
check("Q-011", "images.load(missing) returns nil",
      imgBad == nil)

-- ── Sequences (I-1467..I-1469) ────────────────────────────────────────────
for _, fn in ipairs({"loadGif","loadSequence"}) do
    check("Q-020." .. fn, "bbfx.sequences." .. fn .. " is a function",
          type(bbfx.sequences[fn]) == "function")
end
-- loadSequence on a missing dir returns a handle with no frames.
local seq = bbfx.sequences.loadSequence("/does/not/exist", "frame_%04d.png", 1, 3)
check("Q-021", "sequences.loadSequence returns a handle",
      type(seq) == "table")
for _, m in ipairs({"setFPS","getFPS","setLoop","play","pause","stop",
                    "update","release","getTextureName","frameCount",
                    "currentIndex","isPlaying","backend"}) do
    check("Q-022." .. m, "sequence." .. m .. " is a function",
          type(seq[m]) == "function")
end
check("Q-023", "empty sequence frameCount is 0",
      seq.frameCount() == 0)
check("Q-024", "sequence.update(0.1) does not throw on empty",
      pcall(function() seq.update(0.1) end))

-- loadGif returns a handle (whose backend is 'Null' if stb missing).
local gif = bbfx.sequences.loadGif("/does/not/exist.gif")
check("Q-025", "sequences.loadGif returns a handle",
      type(gif) == "table")
check("Q-026", "gif backend is a string",
      type(gif.backend()) == "string")

-- ── Models (I-1470..I-1471) ───────────────────────────────────────────────
for _, fn in ipairs({"import","isAvailable"}) do
    check("Q-030." .. fn, "bbfx.models." .. fn .. " is a function",
          type(bbfx.models[fn]) == "function")
end
check("Q-031", "models.isAvailable returns a boolean",
      type(bbfx.models.isAvailable()) == "boolean")

-- Importing a missing file returns nil.
local meshBad = bbfx.models.import("/does/not/exist.obj")
check("Q-032", "models.import(missing) returns nil",
      meshBad == nil)

-- ── I-1475 : non-regression Theora video still works ─────────────────────
-- Theora-specific infrastructure (TheoraClip + TheoraClipNode) is
-- compiled into bbfx-core. We verify the Animator singleton is still
-- reachable so that code paths that create a TheoraClip at runtime
-- still work — the actual TheoraClipNode type is a NodeTypeRegistry
-- entry that only exists in Studio (registered by StudioApp). In
-- headless we can only verify the core prerequisite survived.
check("Q-050", "bbfx.Animator singleton still reachable (Theora prerequisite)",
      type(bbfx.Animator) == "table" or type(bbfx.Animator) == "userdata")

-- ── Non-regression : Lot P/O bindings still reachable ────────────────────
check("Q-099.a", "bbfx.json.encode still reachable",
      type(bbfx.json.encode) == "function")
check("Q-099.b", "bbfx.noise.simplex2D still reachable",
      type(bbfx.noise.simplex2D) == "function")
check("Q-099.c", "bbfx.timeline.create still reachable",
      type(bbfx.timeline.create) == "function")

print("\n--------------------------------------------------------------")
print(string.format("  Lot Q Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot Q tests FAILED")
end

os.exit(0)
