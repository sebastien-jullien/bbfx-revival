-- demo_video.lua — BBFx v2.4 Video Demo
-- Plays bombe.ogg as a dynamic texture on a billboard

require 'helpers'
require 'object'
require 'effect'
require 'video'

bbfx_globals()

print("[demo_video] BBFx Theora Video Demo")
print("  P: Play/Pause")
print("  R: Reverse (if available)")
print("  S: Stop")
print("  ESC: Quit")

-- Scene setup
Effect.ambient(0.3, 0.3, 0.3)
Effect.bg(Ogre.ColourValue(0.1, 0.1, 0.2))

-- Light
local lightObj = Object:fromLight(Ogre.Light.LT_POINT, Ogre.ColourValue(1, 1, 1))
lightObj:setPosition(Ogre.Vector3(100, 200, 100))

-- Camera
local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode(UID("cam/"))
camNode:attachObject(cam)
camNode:setPosition(Ogre.Vector3(0, 0, 300))
camNode:lookAt(Ogre.Vector3(0, 0, 0), 2)

-- Try to load video
local clip = nil
local ok, err = pcall(function()
    clip = Video:createClip("bombe.ogg")
end)

if clip then
    -- Create billboard with video texture
    local videoObj = Video:overlay(clip, cam, 30, 22)
    videoObj:setPosition(Ogre.Vector3(0, 0, 0))
    clip:play()
    print("[demo_video] Video playing: bombe.ogg")
else
    print("[demo_video] Could not load bombe.ogg: " .. tostring(err))
    print("[demo_video] Falling back to static scene")
    -- Fallback: show ogrehead
    local head = Object:fromMesh("ogrehead.mesh")
end

-- Animation
local tn = bbfx.RootTimeNode.instance()
local animator = bbfx.Animator.instance()

local updateNode = bbfx.LuaAnimationNode("videoUpdate", function(self)
    local dtPort = self:getInput("dt")
    if not dtPort then return end
    local dt = dtPort:getValue()

    -- Update video clip
    if clip then
        clip:frameUpdate(dt)
    end

    -- Keyboard controls
    if keyboard:wasKeyPressed(112) then -- 'p'
        if clip then
            if clip:isPlaying() then
                clip:pause()
                print("[demo_video] Paused")
            else
                clip:play()
                print("[demo_video] Playing")
            end
        end
    end
    if keyboard:wasKeyPressed(115) then -- 's'
        if clip then
            clip:stop()
            print("[demo_video] Stopped")
        end
    end
end)
updateNode:addInput("dt")
animator:addNode(tn)
animator:addNode(updateNode)
animator:addPort(tn, "dt", updateNode, "dt")

print("[demo_video] Press ESC to exit")
