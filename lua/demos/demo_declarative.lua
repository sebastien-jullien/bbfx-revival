-- demo_declarative.lua — BBFx v2.5 Declarative Graph Demo
-- Builds an LFO -> Ramp -> PerlinFxNode graph in declarative style

require 'helpers'
require 'object'
require 'effect'
require 'declarative'

bbfx_globals()

print("[demo_declarative] BBFx v2.5 Declarative Graph Demo")
print("  ESC: Quit")
print("  [/]: Decrease/increase LFO frequency")

-- Scene
Effect.ambient(0.5, 0.5, 0.5)
Effect.bg(Ogre.ColourValue(0.05, 0.05, 0.1))

local cam = scene:getCamera("MainCamera")
local camNode = scene:getRootSceneNode():createChildSceneNode(UID("cam/"))
camNode:attachObject(cam)
camNode:setPosition(Ogre.Vector3(0, 0, 300))
camNode:lookAt(Ogre.Vector3(0, 0, 0), 2)

-- Create a geosphere mesh with PerlinFxNode
local head = Object:fromMesh("ogrehead.mesh")

-- Create PerlinFxNode
local perlinFx = bbfx.PerlinFxNode(head.animNode:getName() .. "_perlin",
    scene, head.movable)

-- Build graph declaratively (< 15 lines)
local handles = build({
    nodes = {
        {name="lfo",    type="LFO",      frequency=0.5, amplitude=30.0, waveform=0},
        {name="ramp",   type="Ramp",     rate=3.0},
        {name="perlin", type="existing", ref=perlinFx},
    },
    links = {
        {from="lfo.out",  to="ramp.target"},
        {from="ramp.out", to="perlin.displacement"},
    }
})

-- Wire dt to lfo
local animator = bbfx.Animator.instance()
local tn = bbfx.RootTimeNode.instance()
animator:addNode(tn)
local lfo_raw = handles.nodes.lfo._node
animator:addPort(tn, "dt", lfo_raw, "dt")
local ramp_raw = handles.nodes.ramp._node
animator:addPort(tn, "dt", ramp_raw, "dt")
animator:addNode(perlinFx)

print("[demo_declarative] Graph built declaratively:")
print("  LFONode (sin 0.5Hz) -> RampNode (rate=3) -> PerlinFxNode")
print("  handles.nodes.lfo = " .. tostring(handles.nodes.lfo))

-- Keyboard control node
local updateNode = bbfx.LuaAnimationNode("declDemo_kbd", function(self_node)
    local dtPort = self_node:getInput("dt")
    if not dtPort then return end
    if keyboard:wasKeyPressed(91) then -- '['
        local port = lfo_raw:getInput("frequency")
        if port then port:setValue(math.max(0.05, port:getValue() - 0.1)) end
        print("[demo_declarative] LFO freq: " .. lfo_raw:getInput("frequency"):getValue())
    end
    if keyboard:wasKeyPressed(93) then -- ']'
        local port = lfo_raw:getInput("frequency")
        if port then port:setValue(port:getValue() + 0.1) end
        print("[demo_declarative] LFO freq: " .. lfo_raw:getInput("frequency"):getValue())
    end
end)
updateNode:addInput("dt")
animator:addNode(updateNode)
animator:addPort(tn, "dt", updateNode, "dt")
