-- demo_shell.lua — BBFx v2.6 "Shell & Scripting" Interactive Demo
-- REPL console + TCP shell server (port 33195) + hot reload

require 'helpers'
require 'object'
require 'effect'
require 'temporal_nodes'
require 'declarative'
require 'console'
require 'shell/server'
require 'hotreload'

bbfx_globals()

print("=== BBFx v2.6 — Shell & Scripting Demo ===")
print("")
print("Interactive features:")
print("  REPL console active — type Lua expressions below")
print("  TCP shell server on port 33195")
print("    Connect: python lua/shell/client.py")
print("    Or: echo 'return 1+1' | nc 127.0.0.1 33195")
print("  Hot reload: watch('script.lua') to auto-reload on change")
print("")
print("Commands: graph(), ports('name'), set('name','port',val),")
print("          reload('script'), watch('file'), unwatch('file'),")
print("          watchlist(), help(), quit()")
print("")
print("  ESC: Quit")
print("")

-- Scene setup
Effect.ambient(0.4, 0.4, 0.4)
Effect.bg(Ogre.ColourValue(0.04, 0.04, 0.1))

local lightObj = Object:fromLight(Ogre.Light.LT_POINT, Ogre.ColourValue(1, 1, 1))
lightObj:setPosition(Ogre.Vector3(200, 300, 200))

local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode(UID("cam/"))
camNode:attachObject(cam)
camNode:setPosition(Ogre.Vector3(0, 0, 300))
camNode:lookAt(Ogre.Vector3(0, 0, 0), 2)

-- Geosphere with Perlin + LFO
local head = Object:fromMesh("ogrehead.mesh")
local perlinFx = bbfx.PerlinFxNode(head.animNode:getName() .. "_perlin", scene, head.movable)

-- Declarative graph: LFO → Ramp → Perlin
local handles = build({
    nodes = {
        {name="lfo",    type="LFO",      frequency=0.5, amplitude=30.0, waveform=0},
        {name="ramp",   type="Ramp",     rate=2.0},
        {name="perlin", type="existing", ref=perlinFx},
    },
    links = {
        {from="lfo.out",  to="ramp.target"},
        {from="ramp.out", to="perlin.displacement"},
    }
})

local animator = bbfx.Animator.instance()
local tn = bbfx.RootTimeNode.instance()
local lfo_raw  = handles.nodes.lfo._node
local ramp_raw = handles.nodes.ramp._node
animator:addPort(tn, "dt", lfo_raw,  "dt")
animator:addPort(tn, "dt", ramp_raw, "dt")
animator:addNode(perlinFx)

-- Start TCP shell server
local shell = ShellServer:new()

-- Rotation node
local rotNode = bbfx.LuaAnimationNode(UID("rot/"), function(self)
    local dtPort = self:getInput("dt")
    if not dtPort then return end
    head.node:yaw(Ogre.Radian(dtPort:getValue() * 0.4))
end)
rotNode:addInput("dt")
animator:addNode(rotNode)
animator:addPort(tn, "dt", rotNode, "dt")

Logger.info("[demo_shell] Ready — type help() for commands")
