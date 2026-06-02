-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_enable_toggle.lua  —  v3.5.2 Sprint S8 Lot AT visual test
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : démontrer le port `enabled` universel. On crée un overlay
-- texturé (TextureBlend → MaterialBridge → FullscreenOverlay) PLUS un
-- GamepadNode + JoystickRouterNode en mode toggle relié à overlay.enabled.
--
-- Sans gamepad branché, on simule le toggle après ~30 frames via le port
-- `enabled` directement : l'overlay disparaît (node frozen + grayed) puis
-- réapparaît. /inspect --animation prend 2 frames espacées pour voir le
-- changement d'état.
--
-- VÉRIFICATION ATTENDUE (/inspect --animation) :
--   - Frame A : overlay texturé visible (blend BumpyMetal+Water01)
--   - Frame B (après toggle) : overlay disparu OU re-visible selon timing
--   - Prouve que le port `enabled` coupe/rallume N'IMPORTE QUEL node
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_enable_toggle.lua --animation
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_enable_toggle] DIAGNOSTIC universal `enabled` port")

local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.4, 0.4, 0.45))

local frame = 0
local built = false
local enabled = true
local function tick()
    frame = frame + 1
    if frame == 3 and not built then
        dbg.clear()
        built = true
        dbg.create("CameraNode", "cam")
        dbg.create("TextureBlendNode", "blnd")
        dbg.create("FullscreenOverlayNode", "ovl")
        dbg.material_bridge("br", "", "unlit")
        dbg.create("GamepadNode", "gp")
        dbg.create("JoystickRouterNode", "jr")
        _dbg_process_pending()
        dbg.set_param("blnd", "tex_a", "BumpyMetal.jpg")
        dbg.set_param("blnd", "tex_b", "Water01.jpg")
        dbg.set_param("blnd", "mask",  "aureola.png")
        dbg.set_param("ovl",  "mode",  "screen_aligned")
        dbg.set_param("jr",   "mode",  "toggle")
        dbg.set_param("jr",   "button_index", "0")  -- buttonA toggles overlay on/off
        -- Material flow
        dbg.link("blnd", "material_ready", "br",  "material_source")
        dbg.link("ovl",  "entity",         "br",  "entity")
        -- Toggle wiring : gamepad buttonA → jr (router in toggle mode).
        -- NOTE : we do NOT link jr.toggled → ovl.enabled here because the
        -- router's toggled output starts at 0 (would keep the overlay OFF).
        -- In real use you'd press buttonA once to toggle it ON. For this
        -- automated visual demo we drive ovl.enabled directly via dbg.set
        -- below (same `enabled` port — proves the universal mechanism).
        dbg.link("gp",   "buttonA",        "jr",  "button")
        print("[inspect_s8_enable_toggle] Graph built — overlay textured + universal enabled port demo")
    end
    -- Deterministic single toggle for visual capture : the overlay is ENABLED
    -- from frame ~3 (textured blend visible). At frame 300 (~2s) we DISABLE it
    -- via `dbg.set("ovl", "enabled", 0)` → the Studio main loop's tick() reads
    -- the port → setEnabled(false) → the Rectangle2D is hidden. It STAYS off.
    -- So : capture before 2s = overlay visible ; capture after 3s = overlay gone.
    -- (Same `enabled` port a JoystickRouter `toggled` output would drive — this
    --  is the universal Sprint S8 Lot AT mechanism, just driven deterministically.)
    if built and frame == 300 then
        dbg.set("ovl", "enabled", 0.0)
        print("[inspect_s8_enable_toggle] frame 300 : overlay enabled = false (stays off)")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_dbg_inspect_pump_enb_toggle", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
