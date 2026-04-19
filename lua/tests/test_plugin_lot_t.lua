-- ============================================================================
-- BBFx v3.5 Lot T — RenderTexture / FrameBuffer / Compositor Lua API
-- ============================================================================
-- Coverage : I-1504..I-1509
--   * bbfx.renderTexture.create + handle (setCamera/readPixels/release/update)
--   * bbfx.frameBuffer.saveToFile / getPixel / getResolution
--   * bbfx.compositor.enable / disable / listAvailable / registerCustom
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot T — RenderTexture / FrameBuffer / Compositor")
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

-- ── Namespaces ─────────────────────────────────────────────────────────
for _, ns in ipairs({"renderTexture","frameBuffer","compositor"}) do
    check("T-001." .. ns, "bbfx." .. ns .. " namespace exists",
          type(bbfx[ns]) == "table")
end

-- ── renderTexture.create (I-1504/1505) ────────────────────────────────
check("T-002", "bbfx.renderTexture.create is a function",
      type(bbfx.renderTexture.create) == "function")

local rt = bbfx.renderTexture.create("bbfx_test_rt_t", 64, 64,
    { format = "RGBA8", msaa = 0, depthBuffer = true })
check("T-003", "create returns a handle table",
      type(rt) == "table")
for _, m in ipairs({"getTextureName","getWidth","getHeight",
                    "setCamera","update","readPixels","release"}) do
    check("T-004." .. m, "rt." .. m .. " is a function",
          type(rt[m]) == "function")
end
check("T-005", "getTextureName round-trips",
      rt.getTextureName() == "bbfx_test_rt_t")
check("T-006", "getWidth returns 64",
      rt.getWidth() == 64)

-- setCamera against a missing camera returns false — we don't have a
-- live scene in headless, so this is the expected path.
check("T-007", "setCamera(missing camera) returns false",
      rt.setCamera("no_such_cam") == false)

-- update() must not throw even if the viewport has no camera.
local okUpdate = pcall(function() rt.update() end)
check("T-008", "rt.update() does not throw on inert viewport",
      okUpdate == true)

-- readPixels : returns a table of values (may be empty on inert RT;
-- the contract is "no crash" + "returns a table").
local pixels = rt.readPixels()
check("T-009", "readPixels returns a table",
      type(pixels) == "table")

rt.release()
check("T-010", "release() is callable", true)

-- Bad inputs.
local rtBad = bbfx.renderTexture.create("bbfx_test_rt_bad", 0, 0)
check("T-011", "create(0, 0) returns nil",
      rtBad == nil)

-- ── frameBuffer (I-1507) ──────────────────────────────────────────────
for _, fn in ipairs({"saveToFile","getPixel","getResolution"}) do
    check("T-020." .. fn, "bbfx.frameBuffer." .. fn .. " is a function",
          type(bbfx.frameBuffer[fn]) == "function")
end

-- getResolution of a known texture — we create an empty RT first.
local rt2 = bbfx.renderTexture.create("bbfx_test_rt_fb", 32, 32)
local w, h = bbfx.frameBuffer.getResolution("bbfx_test_rt_fb")
check("T-021", "getResolution('bbfx_test_rt_fb') = 32, 32",
      w == 32 and h == 32)

-- getPixel on an unknown texture returns (0, 0, 0, 0).
local r, g, b, a = bbfx.frameBuffer.getPixel("no_such_tex", 0, 0)
check("T-022", "getPixel(unknown) returns (0, 0, 0, 0)",
      r == 0 and g == 0 and b == 0 and a == 0)

-- getPixel out-of-range returns zeros.
local r2, g2, b2, a2 = bbfx.frameBuffer.getPixel("bbfx_test_rt_fb", 999, 999)
check("T-023", "getPixel(out of range) returns (0, 0, 0, 0)",
      r2 == 0 and g2 == 0 and b2 == 0 and a2 == 0)

-- saveToFile of an empty RT should write a PNG to disk (OGRE saves
-- whatever pixels are in the buffer — uninitialized content on inert
-- texture, but the file path is valid).
local tmp = (os.getenv("TEMP") or os.getenv("TMP") or "/tmp"):gsub("[/\\]+$", "")
local outPath = tmp .. "/bbfx_lot_t_save.png"
local okSave = bbfx.frameBuffer.saveToFile(outPath, "bbfx_test_rt_fb")
check("T-024", "saveToFile returns true on a valid source",
      okSave == true)
check("T-025", "file exists after saveToFile",
      bbfx.fs.exists(outPath))
os.remove(outPath)

rt2.release()

-- ── compositor (I-1506) ───────────────────────────────────────────────
for _, fn in ipairs({"enable","disable","listAvailable","registerCustom"}) do
    check("T-030." .. fn, "bbfx.compositor." .. fn .. " is a function",
          type(bbfx.compositor[fn]) == "function")
end
-- listAvailable returns a table (may be empty in headless but non-nil).
local compList = bbfx.compositor.listAvailable()
check("T-031", "compositor.listAvailable returns a table",
      type(compList) == "table")

-- enable/disable with a bogus name returns false cleanly (no crash).
check("T-032", "compositor.enable(missing) = false",
      bbfx.compositor.enable("no_such_comp") == false)
check("T-033", "compositor.disable(missing) = false",
      bbfx.compositor.disable("no_such_comp") == false)

-- registerCustom accepts a minimal compositor script.
local miniScript = [[
compositor bbfx_test_comp_t {
    technique {
        texture scene target_width target_height PF_A8R8G8B8
        target scene { input previous }
        target_output { input none
            pass render_quad { material BaseWhiteNoLighting }
        }
    }
}
]]
local okReg = bbfx.compositor.registerCustom("bbfx_test_comp_t", miniScript)
check("T-034", "compositor.registerCustom returns a boolean",
      type(okReg) == "boolean")

-- ── Non-regression : Lot S/R/Q bindings still reachable ───────────────
check("T-099.a", "bbfx.authoring.writePlugin still reachable",
      type(bbfx.authoring.writePlugin) == "function")
check("T-099.b", "bbfx.sdf.sphere still reachable",
      type(bbfx.sdf.sphere) == "function")
check("T-099.c", "bbfx.media.openVideo still reachable",
      type(bbfx.media.openVideo) == "function")

print("\n--------------------------------------------------------------")
print(string.format("  Lot T Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot T tests FAILED")
end

os.exit(0)
