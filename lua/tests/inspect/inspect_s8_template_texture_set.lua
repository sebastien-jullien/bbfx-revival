-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_template_texture_set.lua  —  Lot AF template visual test
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : valider visuellement le template `vj_texture_set_classic`
-- créé en Sprint S8 Lot AF (CDC OBJ-352-115). Reproduction structurelle
-- du Fanions 2006 (BPM 144) : 2 cycles + blend + bridge + overlay.
--
-- VÉRIFICATION ATTENDUE (/inspect screenshot) :
--   - Quad plein écran avec material du TextureBlend (BumpyMetal + NMBalls)
--   - Sweep mask vertical aureola.png appliqué
--   - PAS de blanc (BaseWhite)
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_template_texture_set.lua
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_template_texture_set] Loading vj_texture_set_classic template...")

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
        print("[inspect_s8_template_texture_set] dofile vj_texture_set_classic")
        local t = dofile("lua/templates/vj_texture_set_classic.lua")
        if t and t.setup then t.setup() end
        print("[inspect_s8_template_texture_set] Template loaded — graph alive")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_tpl_classic", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
