-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_multibank.lua  —  v3.5.2 Sprint S8 visual regression test
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : valider visuellement le préset multibank_chamber corrigé
-- en Sprint S8 Lot AE. Vérifie que la chaîne :
--   MultiTextureBank → TextureBlend → MaterialBridge → FullscreenOverlay
-- produit une image (PAS BaseWhite).
--
-- VÉRIFICATION ATTENDUE (par /inspect screenshot) :
--   - Quad plein écran avec material du TextureBlend (BumpyMetal + Water01
--     blendés en alpha avec masque aureola.png)
--   - PAS de blanc uniforme
--   - Texture métal + eau visible
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_multibank.lua
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_multibank] Loading multibank_chamber preset...")

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
        print("[inspect_s8_multibank] dbg.preset('multibank_chamber')")
        dbg.preset("multibank_chamber")
        print("[inspect_s8_multibank] Preset queued — graph alive for inspection")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_multibank", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
