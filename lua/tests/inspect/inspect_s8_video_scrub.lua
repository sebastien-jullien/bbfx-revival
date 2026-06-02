-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_video_scrub.lua  —  v3.5.2 Sprint S8 visual test
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : valider visuellement le préset video_scrub_loop corrigé en
-- Sprint S8 Lot AE. La chaîne :
--   TheoraClipNode → VideoSlicerNode → MaterialBridge → FullscreenOverlay
-- doit afficher le clip vidéo bombe.ogg sur l'overlay plein écran.
--
-- VÉRIFICATION ATTENDUE (/inspect screenshot --animation IDÉAL) :
--   - PAS de blanc — clip vidéo bombe.ogg visible
--   - Frame du clip (image vidéo, pas le default BaseWhite)
--   - Si --animation : variation de la frame entre 2 captures (auto_play)
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_video_scrub.lua --animation
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_video_scrub] Loading video_scrub_loop preset...")

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
        print("[inspect_s8_video_scrub] dbg.preset('video_scrub_loop')")
        dbg.preset("video_scrub_loop")
        print("[inspect_s8_video_scrub] Preset queued — Theora → Slicer → overlay")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_scrub", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
