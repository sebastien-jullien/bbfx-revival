-- demo_production.lua — BBFx v2.9 "Production" Demo
-- Pipeline: record session → replay offline → export PNG

package.path = "lua/?.lua;" .. package.path

require 'helpers'
require 'object'
require 'effect'
require 'shader'
require 'audio'
require 'recorder'
require 'player'
require 'exporter'
require 'functional'
require 'console'

bbfx_globals()

print("=== BBFx v2.9 — Production Demo ===")
print("")
print("  Pipeline: Live → Record → Replay → Export PNG")
print("")
print("Controls:")
print("  R      : Start/stop recording inputs")
print("  P      : Replay last session (offline mode)")
print("  E      : Export frames during replay")
print("  H      : Toggle HUD")
print("  ESC    : Quit")
print("")
print("Pipeline steps:")
print("  1. Press R to start recording")
print("  2. Move mouse/press keys during recording")
print("  3. Press R again to stop")
print("  4. Press P to replay in offline mode")
print("  5. Press E to export PNG frames during replay")
print("")

-- Scene
Effect.ambient(0.4, 0.4, 0.4)
Effect.bg(Ogre.ColourValue(0.03, 0.03, 0.08))

local lightObj = Object:fromLight(Ogre.Light.LT_POINT, Ogre.ColourValue(1, 1, 1))
lightObj:setPosition(Ogre.Vector3(200, 300, 200))

local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode(UID("cam/"))
camNode:attachObject(cam)
camNode:setPosition(Ogre.Vector3(0, 0, 300))
camNode:lookAt(Ogre.Vector3(0, 0, 0), 2)

local perlinFx = bbfx.PerlinFxNode("ogrehead.mesh", "prod_perlin")
perlinFx:enable()
local head = Object:fromMesh("prod_perlin")

-- Audio (optional)
local audioInst = Audio:start()
_G._bbfx_audio = audioInst

-- State
local recording = false
local sessionFile = "session.bbfx-session"

-- Main update
local animator = bbfx.Animator.instance()
local tn = bbfx.RootTimeNode.instance()
animator:addNode(perlinFx)

local updateNode = bbfx.LuaAnimationNode(UID("prodDemo/"), function(self)
    local dtPort = self:getInput("dt")
    if not dtPort then return end
    local dt = dtPort:getValue()

    head.node:yaw(Ogre.Radian(dt * 0.3))

    -- R: toggle record
    if keyboard:wasKeyPressed(114) then
        if not recording then
            record(sessionFile)
            recording = true
            print("[demo] Recording started...")
        else
            stoprecord()
            recording = false
            print("[demo] Recording stopped.")
        end
    end

    -- P: replay offline
    if keyboard:wasKeyPressed(112) then
        bbfx.Engine.instance():setOfflineMode(60)
        replay(sessionFile)
        print("[demo] Replaying in offline mode (60fps)...")
    end

    -- E: toggle export
    if keyboard:wasKeyPressed(101) then
        if export_toggle("output/frames") then
            print("[demo] Exporting PNG frames...")
        else
            print("[demo] Export stopped.")
        end
    end
end)
updateNode:addInput("dt")
animator:addNode(updateNode)
animator:addPort(tn, "dt", updateNode, "dt")

-- Functional test
local doubled = map({1,2,3,4,5}, function(x) return x * 2 end)
print("[functional] map({1..5}, x*2) = " .. table.concat(doubled, ","))

print("[demo_production] Ready.")
print("bbfx> ")
