-- ─────────────────────────────────────────────────────────────────────
-- inspect_s8_spectrogram.lua  —  v3.5.2 Sprint S8 visual test
-- ─────────────────────────────────────────────────────────────────────
-- OBJECTIF : valider visuellement le préset spectrogram_displacement
-- (alias historique : audio_displaced_geosphere) corrigé en Sprint S8
-- Lot AE. La chaîne :
--   AudioCapture → SpectrogramTexture → MaterialBridge → SceneObject (mesh)
-- doit appliquer la texture spectrogramme (colormap viridis) sur la mesh.
--
-- VÉRIFICATION ATTENDUE (/inspect screenshot --animation IDÉAL) :
--   - Mesh (ogrehead par défaut) avec texture spectrogramme appliquée
--   - Couleurs viridis (vert/jaune/violet) sur la mesh
--   - Si --animation + son ambiant : scroll horizontal du spectrogramme
--
-- LANCEMENT :
--   /inspect studio lua/tests/inspect/inspect_s8_spectrogram.lua --animation
-- ─────────────────────────────────────────────────────────────────────

print("[inspect_s8_spectrogram] Loading spectrogram_displacement preset...")

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
        print("[inspect_s8_spectrogram] dbg.preset('spectrogram_displacement')")
        dbg.preset("spectrogram_displacement")
        print("[inspect_s8_spectrogram] Preset queued — audio → spectro → mesh chain")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_spectro", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
