-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_fanions.lua  —  v3.5.2 Sprint S8 visual regression test
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : valider visuellement le préset Fanions corrigé en Sprint S8
-- Lot AE. Avant S8 le FullscreenOverlay affichait BaseWhite (écran blanc) ;
-- après S8 il doit afficher la texture du blend (heritage ou fallback).
--
-- VÉRIFICATION ATTENDUE (par /inspect screenshot) :
--   - ÉCRAN couvert par un quad plein-écran (FullscreenOverlay camera_locked)
--   - PAS de blanc uniforme — la texture du blend (BumpyMetal/Heritage)
--     doit etre visible (motif texturé organique/métallique)
--   - Si --animation : aucun mouvement attendu (preset statique sans input)
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_fanions.lua
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_fanions] Loading fanions_dans_la_plaine preset...")

local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.4, 0.4, 0.45))

-- Frame counter scheduling : on attend 5 frames d'init OGRE/preset avant de
-- charger le preset, puis on laisse tourner indefiniment pour /inspect.
local frame = 0
local loaded = false
local function tick()
    frame = frame + 1
    if frame == 3 and not loaded then
        dbg.clear()  -- v3.5.2 Sprint S8 : clean default Studio scene before test setup
        loaded = true
        print("[inspect_s8_fanions] dbg.preset('fanions_dans_la_plaine')")
        dbg.preset("fanions_dans_la_plaine")
        print("[inspect_s8_fanions] Preset queued — graph alive for inspection")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_fanions", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
