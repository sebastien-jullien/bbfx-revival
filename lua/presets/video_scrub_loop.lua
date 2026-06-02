-- Preset: video_scrub_loop
-- Sprint S4 demo: VideoSlicer + GamepadNode axis -> playhead = direct scrub.
local ParamSpec = require "paramspec"
return {
    name = "video_scrub_loop", version = 2, category = "VJ",  -- v2 = Sprint S8 Lot AE : material flow wired
    description = "Video scrub loop: gamepad axis 0 = direct timeline scrub",
    tags = {"vj", "video", "scrub", "gamepad", "demo"},
    params = ParamSpec.declare({}),
    build = function(params)
        return {
            type = "CompositionNode",
            primary = "overlay",
            nodes = {
                {name="cam",      type="CameraNode"},
                {name="clip",     type="TheoraClipNode"},
                {name="slicer",   type="VideoSlicerNode"},
                {name="overlay",  type="FullscreenOverlayNode",
                  params={mode="screen_aligned"}},
                {name="gamepad",  type="GamepadNode"},
                {name="router",   type="JoystickRouterNode",
                  params={button_index=0, axis_index=0, mode="hold_gate"}},
                -- v3.5.2 Sprint S8 Lot AE : MaterialBridge route slicer.material_out
                -- (i.e. le video clip materialisé scrubbed) vers le FullscreenOverlay.
                {name="bridge",   type="MaterialBridgeNode",
                  params={lighting_mode="unlit"}},
            },
            links = {
                {from="clip",    fromPort="playing",     to="slicer", toPort="clip"},
                {from="gamepad", fromPort="leftStickX",  to="router", toPort="axis"},
                {from="gamepad", fromPort="buttonA",     to="router", toPort="button"},
                {from="router",  fromPort="gated_value", to="slicer", toPort="playhead"},
                -- Material flow : slicer.material_out → bridge → overlay
                {from="slicer",  fromPort="material_ready", to="bridge",  toPort="material_source"},
                {from="overlay", fromPort="entity",         to="bridge",  toPort="entity"},
            },
            params = params,
        }
    end
}
