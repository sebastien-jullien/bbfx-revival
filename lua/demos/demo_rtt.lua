-- ============================================================================
-- BBFx v3.5 Lot T — Render-to-Texture demo
-- ============================================================================
-- Creates a 512x512 RenderTexture, binds the main scene camera to it,
-- captures the frame, and saves a PNG snapshot.
-- ============================================================================

print("=== BBFx RTT Demo (Lot T) ===\n")

local rt = bbfx.renderTexture.create("demo_rt", 512, 512, {
    format = "RGBA8",
    msaa = 0,
    depthBuffer = true,
})
if not rt then
    print("Failed to create RenderTexture.")
    return
end
print("RT created: " .. rt.getTextureName()
      .. " (" .. rt.getWidth() .. "x" .. rt.getHeight() .. ")")

-- Bind the main camera if available (scene name depends on project —
-- demo_studio.lua uses "main_camera" by convention).
if rt.setCamera("main_camera") then
    print("Camera 'main_camera' bound to RT.")
    rt.update()
    -- Save snapshot to temp.
    local tmp = (os.getenv("TEMP") or os.getenv("TMP") or "/tmp"):gsub("[/\\]+$","")
    local p = tmp .. "/bbfx_rtt_demo.png"
    if bbfx.frameBuffer.saveToFile(p, "demo_rt") then
        print("Saved snapshot: " .. p)
    end
else
    print("No 'main_camera' in scene — RT stays unbound.")
end

-- Read a pixel (center).
local r, g, b, a = bbfx.frameBuffer.getPixel("demo_rt", 256, 256)
print(string.format("Center pixel RGBA: (%d, %d, %d, %d)", r, g, b, a))

-- Compositor enumeration.
local list = bbfx.compositor.listAvailable()
print(string.format("Compositors available: %d", #list))
for i = 1, math.min(3, #list) do print("  - " .. list[i]) end

-- Cleanup.
rt.release()
print("\nDemo done.")
