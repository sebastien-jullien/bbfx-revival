-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_theora_geosphere.lua  —  Sprint S5 baseline visual test
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : baseline Sprint S5 — préset theora_on_geosphere. Ce préset
-- était DEJA fonctionnel pre-Sprint-S8 (chaîne TheoraClip → MaterialBridge
-- → SceneObjectNode Geosphere). Il sert de REFERENCE pour comparer le
-- comportement multi-target (FullscreenOverlay/BillboardLayer) ajouté
-- en Lot AC à la baseline SceneObjectNode classique.
--
-- VÉRIFICATION ATTENDUE (/inspect screenshot) :
--   - Geosphere 3D rendue au centre de l'écran
--   - Sub-entity material remplacé par le clip video (sphère "vidéo-mapped")
--   - PAS de couleur par défaut du mesh (gray/white) — la vidéo bombe.ogg
--     doit etre la texture de la sphère.
--   - Si --animation : la vidéo joue (frame N≠N+30).
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_theora_geosphere.lua --animation
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_theora_geosphere] Loading theora_on_geosphere (S5 baseline)...")

local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.45, 0.45, 0.5))

local frame = 0
local loaded = false
local function tick()
    frame = frame + 1
    if frame == 3 and not loaded then
        dbg.clear()  -- v3.5.2 Sprint S8 : clean default Studio scene before test setup
        loaded = true
        print("[inspect_s8_theora_geosphere] dbg.preset('theora_on_geosphere')")
        dbg.preset("theora_on_geosphere")
        print("[inspect_s8_theora_geosphere] Preset queued — S5 baseline reference")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_theora", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
