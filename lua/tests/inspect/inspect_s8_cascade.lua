-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_cascade.lua  —  Lot AD cross-class cascade visual test
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : valider visuellement le Pattern 4 mApplySeq cross-class
-- (Sprint S8 Lot AD). Setup : MaterialNode + MaterialBridge ciblant la
-- même SceneObjectNode (Geosphere). Le dernier connecté gagne.
--
-- Cas testé : MaterialNode connecté en PREMIER (apparait avec material A),
-- puis MaterialBridge connecté en SECOND (doit override avec material B).
--
-- VÉRIFICATION ATTENDUE (/inspect screenshot) :
--   - Geosphere 3D au centre
--   - Material visible = matB (BBFx/Chrome) car MaterialBridge connecté
--     EN SECOND (mApplySeq[B] > mApplySeq[A])
--   - PAS material A (BBFx/Hologram qui aurait gagné sans cascade)
--
-- VARIANT --animation : disable du MaterialBridge après 3s ; la
-- MaterialNode reasserts. Détectable si /inspect prend 2 frames espacées.
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_cascade.lua
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_cascade] Setting up cross-class cascade MaterialNode + MaterialBridge...")

local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.5, 0.5, 0.55))

local frame = 0
local built = false

local matA = "BBFx/Hologram"   -- premier (MaterialNode)
local matB = "BBFx/Chrome"     -- second (MaterialBridge)

local function buildGraph()
    -- 1) Camera + Geosphere
    dbg.create("CameraNode", "cam")
    dbg.create_with_param("SceneObjectNode", "geo", "mesh", "Geosphere8000.mesh")
    -- 2) MaterialNode connecté EN PREMIER (mApplySeq plus bas)
    dbg.create("MaterialNode", "matnode")
    dbg.set_param("matnode", "material", matA)
    -- v3.5.2 S8 : flush pending creates BEFORE first link
    _dbg_process_pending()
    dbg.link("geo", "entity", "matnode", "entity")
    print(string.format("[inspect_s8_cascade] Step 1 : MaterialNode → geo (matA=%s)", matA))
    -- 3) MaterialBridge connecté EN SECOND (mApplySeq plus haut, doit override)
    dbg.material_bridge("mbr", matB, "unlit")
    _dbg_process_pending()
    dbg.link("geo", "entity", "mbr", "entity")
    print(string.format("[inspect_s8_cascade] Step 2 : MaterialBridge → geo (matB=%s) — should win", matB))
end

local function tick()
    frame = frame + 1
    if frame == 3 and not built then
        dbg.clear()  -- v3.5.2 Sprint S8 : clean default Studio scene before test setup
        built = true
        buildGraph()
        print("[inspect_s8_cascade] Graph alive — Geosphere should display matB (Chrome)")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_cascade", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
