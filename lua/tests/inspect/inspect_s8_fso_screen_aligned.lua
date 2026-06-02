-- DIAGNOSTIC : FullscreenOverlayNode en mode `screen_aligned` (Rectangle2D NDC).
-- Si ce mode rend correctement, le bug est isolé au mode `camera_locked`.

print("[inspect_s8_fso_screen_aligned] DIAGNOSTIC : screen_aligned mode")

local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()
scene:setAmbientLight(ColourValue(0.4, 0.4, 0.45))

local frame = 0
local built = false
local function tick()
    frame = frame + 1
    if frame == 3 and not built then
        dbg.clear()
        built = true
        -- FullscreenOverlay direct en mode screen_aligned avec material RustySteel
        dbg.create("FullscreenOverlayNode", "fso_screen")
        _dbg_process_pending()
        dbg.set_param("fso_screen", "mode",     "screen_aligned")
        dbg.set_param("fso_screen", "material", "BBFx/Chrome")
        print("[inspect_s8_fso_screen_aligned] FSO created in screen_aligned mode with BBFx/Chrome")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_fso_screen", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
