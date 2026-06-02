-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_consumer_port.lua  —  Lot AC port material_source consumer
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : isoler visuellement le port `material_source` consumer
-- natif ajouté sur FullscreenOverlayNode (Pattern 3). Sans
-- MaterialBridgeNode intermédiaire, le FullscreenOverlay pulle
-- directement le mirror upstream (TextureBlend.material_out) et fait
-- son propre auto-wrap si besoin.
--
-- GRAPHE CONSTRUIT IN-LINE :
--   - TextureBlend "blnd" avec tex_a/tex_b/mask statiques → produit
--     material_out mirror
--   - FullscreenOverlay "fso" avec material_source linké directement
--     à un output port du blend (resolveMaterialFromSource probe via
--     getSourceNodes → spec → "material_out" mirror)
--
-- Link : blnd.material_ready → fso.material_source
--
-- VÉRIFICATION ATTENDUE (/inspect screenshot) :
--   - Quad plein écran avec material du blend visible
--   - PAS de blanc (BaseWhite)
--   - Pas de MaterialBridge dans le graphe — c'est le port natif qui
--     fait le routing.
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_consumer_port.lua
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_consumer_port] Setting up direct material_source consumer...")

local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.4, 0.4, 0.45))

local frame = 0
local built = false

local function buildGraph()
    dbg.create("CameraNode", "cam")
    dbg.create("TextureBlendNode", "blnd")
    dbg.set_param("blnd", "tex_a",      "BumpyMetal.jpg")
    dbg.set_param("blnd", "tex_b",      "Water01.jpg")
    dbg.set_param("blnd", "mask",       "aureola.png")
    dbg.set_param("blnd", "blend_mode", "alpha")
    dbg.create("FullscreenOverlayNode", "fso")
    -- v3.5.2 S8 : flush pending creates BEFORE linking
    _dbg_process_pending()
    -- Direct link (Pattern 3 native consumer, sans MaterialBridge).
    dbg.link("blnd", "material_ready", "fso", "material_source")
    print("[inspect_s8_consumer_port] Graph : blnd.material_out → fso.material_source (DIRECT)")
end

local function tick()
    frame = frame + 1
    if frame == 3 and not built then
        dbg.clear()  -- v3.5.2 Sprint S8 : clean default Studio scene before test setup
        built = true
        buildGraph()
        print("[inspect_s8_consumer_port] Graph alive — expect blend material on quad")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_csm", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
