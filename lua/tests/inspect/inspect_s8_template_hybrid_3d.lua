-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_template_hybrid_3d.lua  —  Lot AF template visual test
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : valider visuellement le template `vj_hybrid_3d_overlay`
-- créé en Sprint S8 Lot AF (CDC OBJ-352-116). Scene 3D (Geosphere +
-- PerlinFx deform) + overlay vidéo plein écran translucide alpha 0.4.
-- BPM 128.
--
-- VÉRIFICATION ATTENDUE (/inspect screenshot --animation IDÉAL) :
--   - Geosphere 3D visible (rotation Perlin)
--   - Quad plein écran vidéo translucide PAR DESSUS la geosphère
--   - Mix visible : la geosphère 3D apparait à travers l'overlay 0.4 alpha
--   - Si --animation : perlin deform anime la geosphère + frame video change
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_template_hybrid_3d.lua --animation
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_template_hybrid_3d] Loading vj_hybrid_3d_overlay template...")

local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.5, 0.5, 0.55))

local frame = 0
local loaded = false
local function tick()
    frame = frame + 1
    if frame == 3 and not loaded then
        dbg.clear()  -- v3.5.2 Sprint S8 : clean default Studio scene before test setup
        loaded = true
        print("[inspect_s8_template_hybrid_3d] dofile vj_hybrid_3d_overlay")
        local t = dofile("lua/templates/vj_hybrid_3d_overlay.lua")
        if t and t.setup then t.setup() end
        print("[inspect_s8_template_hybrid_3d] Template loaded — 3D + video overlay alive")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_tpl_3d", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
