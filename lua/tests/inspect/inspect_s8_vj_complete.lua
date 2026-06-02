-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_vj_complete.lua  —  v3.5.2 Sprint S8 visual regression test
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : valider visuellement le préset vj_complete_show corrigé en
-- Sprint S8 Lot AE. Ce préset est le plus complexe (14+ nodes) :
--   - 2 TheoraClips → VideoCrossfade → bridge_video → overlay_video
--   - MultiTextureBank → TextureBlend → bridge_tex → overlay_tex
--   - AudioCapture → SpectrogramTexture
--   - TextureFeedback (decay 0.85)
--   - Gamepad routing
--
-- VÉRIFICATION ATTENDUE (/inspect screenshot, idéalement --animation) :
--   - 2 overlays empilés (z_offset 0.02 + 0.01) avec materials différents
--   - PAS de blanc — les 2 overlays doivent rendre les videos/textures
--   - Si --animation : possible mouvement video (clip Theora bombe.ogg loop)
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_vj_complete.lua --animation
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_vj_complete] Loading vj_complete_show preset...")

local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.35, 0.35, 0.4))

local frame = 0
local loaded = false
local function tick()
    frame = frame + 1
    if frame == 3 and not loaded then
        dbg.clear()  -- v3.5.2 Sprint S8 : clean default Studio scene before test setup
        loaded = true
        print("[inspect_s8_vj_complete] dbg.preset('vj_complete_show')")
        dbg.preset("vj_complete_show")
        print("[inspect_s8_vj_complete] Preset queued — multi-layer chain alive")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_vj_complete", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
