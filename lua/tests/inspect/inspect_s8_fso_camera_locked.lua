-- DIAGNOSTIC Lot AK : tester si camera_locked rend après le fix BBT_PERPENDICULAR_COMMON.
print("[inspect_s8_fso_camera_locked] DIAGNOSTIC camera_locked mode")

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
        dbg.create("FullscreenOverlayNode", "fso_cam")
        _dbg_process_pending()
        dbg.set_param("fso_cam", "mode",     "camera_locked")
        dbg.set_param("fso_cam", "material", "BBFx/Chrome")
        print("[inspect_s8_fso_camera_locked] FSO created in camera_locked mode")
    end
end

local animator = bbfx.Animator.instance()
local pump = bbfx.LuaAnimationNode("_inspect_pump_fso_cam", function(self) tick() end)
pump:addInput("dt")
animator:addNode(pump)

local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    animator:addPort(rootTime, "dt", pump, "dt")
end
