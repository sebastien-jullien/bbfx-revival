-- ============================================================================
-- BBFx v3.5 Lot Q — Media demo
-- ============================================================================
-- Exercises the full media pipeline :
--   * bbfx.media.openVideo  — ffmpeg subprocess video playback
--   * bbfx.images.load      — static PNG/JPG/BMP/TGA/HDR
--   * bbfx.sequences.loadGif / loadSequence — animated frames
--   * bbfx.models.import    — OBJ/FBX/glTF via Assimp
--
-- Every call is guarded: missing binaries / files just print a note
-- and continue so the demo script is safe on any machine.
-- ============================================================================

print("=== BBFx Media Demo (Lot Q) ===\n")

-- ── Video (ffmpeg subprocess) ───────────────────────────────────────────
print("ffmpeg available : " .. tostring(bbfx.media.ffmpegAvailable()))
local videoPath = "resources/videos/test.mp4"
local clip = bbfx.media.openVideo(videoPath, { width = 1280, height = 720, fps = 30 })
if clip.isOpen() then
    print("Video opened: " .. clip.getTextureName()
          .. " " .. clip.getWidth() .. "x" .. clip.getHeight()
          .. " @ " .. clip.getFPS() .. "fps")
    clip.setLoop(true)
    clip.play()
else
    print("Video skipped (" .. videoPath .. " not found or ffmpeg missing)")
end

-- ── Image ────────────────────────────────────────────────────────────────
local img = bbfx.images.load("resources/textures/test.png")
if img then
    print("Image loaded: " .. img.getTextureName())
else
    print("Image skipped (resources/textures/test.png not found)")
end

-- ── Sequence (PNG frames) ───────────────────────────────────────────────
local seq = bbfx.sequences.loadSequence(
    "resources/sequences/explosion", "frame_%04d.png", 1, 30)
print(string.format("Sequence frames loaded: %d (backend=%s)",
      seq.frameCount(), seq.backend()))
if seq.frameCount() > 0 then
    seq.setFPS(24)
    seq.setLoop(true)
    seq.play()
end

-- ── Animated GIF ────────────────────────────────────────────────────────
local gif = bbfx.sequences.loadGif("resources/sequences/loop.gif")
print(string.format("GIF frames loaded: %d (backend=%s)",
      gif.frameCount(), gif.backend()))
if gif.frameCount() > 0 then
    gif.setLoop(true)
    gif.play()
end

-- ── 3D model (Assimp) ───────────────────────────────────────────────────
print("Assimp available : " .. tostring(bbfx.models.isAvailable()))
local mesh = bbfx.models.import("resources/models/monkey.obj")
if mesh then
    print("Mesh imported: " .. mesh.getMeshName())
else
    print("Mesh import skipped (file not found or Assimp missing)")
end

print("\nDemo ready. Frame updates are driven by the engine loop:")
print("  per-frame: clip.update(dt), seq.update(dt), gif.update(dt)")
