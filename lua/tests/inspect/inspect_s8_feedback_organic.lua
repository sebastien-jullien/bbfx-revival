-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_feedback_organic.lua  —  v3.5.2 Sprint S8 visual test
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : valider visuellement le préset feedback_organic corrigé en
-- Sprint S8 Lot AE. La chaîne :
--   NoiseTexture → TextureFeedback → MaterialBridge → FullscreenOverlay
-- doit produire un effet de feedback non-linéaire avec trail.
--
-- VÉRIFICATION ATTENDUE (/inspect screenshot --animation IDÉAL) :
--   - PAS de blanc — feedback material rendu sur l'overlay
--   - Si --animation : trail/echo visible entre frames (decay 0.92)
--   - Pattern Perlin noise sur l'overlay
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_feedback_organic.lua --animation
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_feedback_organic] Loading feedback_organic preset...")

local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.3, 0.3, 0.35))

local frame = 0
local loaded = false
local function tick()
    frame = frame + 1
    if frame == 3 and not loaded then
        dbg.clear()  -- v3.5.2 Sprint S8 : clean default Studio scene before test setup
        loaded = true
        print("[inspect_s8_feedback_organic] dbg.preset('feedback_organic')")
        dbg.preset("feedback_organic")
        print("[inspect_s8_feedback_organic] Preset queued — Perlin+Feedback chain")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_feedback", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
