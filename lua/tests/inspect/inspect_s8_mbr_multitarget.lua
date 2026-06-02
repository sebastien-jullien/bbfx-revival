-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_mbr_multitarget.lua  —  Lot AC MaterialBridge multi-target
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : isoler visuellement l'extension Sprint S8 Lot AC du
-- MaterialBridgeNode au-delà de SceneObjectNode. Le bridge écrit dans
-- le ParamSpec.material des targets FullscreenOverlay + BillboardLayer
-- (avec backup/restore — Pattern 1 entity-link).
--
-- GRAPHE CONSTRUIT IN-LINE (pas de préset) :
--   - 1 TextureNode "src" produisant BBFx/Chrome material via tex param
--   - 1 MaterialBridge "mbr" recevant material_in "BBFx/Chrome" en static
--   - 1 FullscreenOverlay "fso" cible du bridge (camera_locked)
--
-- Link :   fso.entity → mbr.entity (Pattern 1)
-- Le bridge écrit BBFx/Chrome dans fso.ParamSpec.material ; fso.update()
-- le picke et l'applique au BillboardSet.
--
-- VÉRIFICATION ATTENDUE (/inspect screenshot) :
--   - Quad plein écran avec material BBFx/Chrome (rendu chromé brillant)
--   - PAS de blanc (BaseWhite par défaut)
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_mbr_multitarget.lua
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_mbr_multitarget] Setting up MaterialBridge → FullscreenOverlay...")

local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.4, 0.4, 0.45))

local frame = 0
local built = false

local function buildGraph()
    -- 1) Camera
    dbg.create("CameraNode", "cam")
    -- 2) FullscreenOverlay target
    dbg.create("FullscreenOverlayNode", "fso")
    -- 3) MaterialBridge avec material_in = BBFx/Chrome
    dbg.material_bridge("mbr", "BBFx/Chrome", "unlit")
    -- v3.5.2 S8 : flush pending operations BEFORE linking — nodes need to exist for link()
    _dbg_process_pending()
    -- 4) Link Pattern 1 : fso.entity → mbr.entity
    dbg.link("fso", "entity", "mbr", "entity")
    print("[inspect_s8_mbr_multitarget] Graph built : fso → mbr(BBFx/Chrome)")
end

local function tick()
    frame = frame + 1
    if frame == 3 and not built then
        dbg.clear()  -- v3.5.2 Sprint S8 : clean default Studio scene before test setup
        built = true
        buildGraph()
        print("[inspect_s8_mbr_multitarget] Graph alive — expect Chrome quad fullscreen")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_mbr_mt", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
