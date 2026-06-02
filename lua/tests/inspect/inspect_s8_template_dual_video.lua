-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_template_dual_video.lua  —  Lot AF template visual test
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : valider visuellement le template `vj_dual_video_crossfade`
-- créé en Sprint S8 Lot AF (CDC OBJ-352-114). Chaîne :
--   2 TheoraClipNodes → VideoCrossfadeNode → MaterialBridge → FullscreenOverlay
--
-- VÉRIFICATION ATTENDUE (/inspect screenshot) :
--   - Quad plein écran avec material du VideoCrossfade
--   - PAS de blanc (BaseWhite) — la vidéo crossfadée doit apparaître
--   - Beta initial = 0 → clip A (bombe.ogg) visible
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_template_dual_video.lua --animation
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_template_dual_video] Loading vj_dual_video_crossfade template...")

local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.4, 0.4, 0.45))

local frame = 0
local loaded = false
local function tick()
    frame = frame + 1
    if frame == 3 and not loaded then
        dbg.clear()  -- v3.5.2 Sprint S8 : clean default Studio scene before test setup
        loaded = true
        print("[inspect_s8_template_dual_video] dofile vj_dual_video_crossfade")
        local t = dofile("lua/templates/vj_dual_video_crossfade.lua")
        if t and t.setup then t.setup() end
        print("[inspect_s8_template_dual_video] Template loaded — graph alive")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_tpl_dual", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
