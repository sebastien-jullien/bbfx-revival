-- Test FINAL : GL3Plus + setClearEveryFrame(false) + Bloom (OGRE natif)
-- + vérification des erreurs OGRE dans les logs
package.path = "lua/?.lua;" .. package.path
require 'helpers'
require 'object'
require 'effect'
require 'camera'
bbfx_globals()

print("=== Test FINAL: GL3Plus + Bloom ===")
Effect.ambient(0.3, 0.3, 0.3)
Effect.bg(Ogre.ColourValue(0.05, 0.05, 0.15))
local headObj = Object:fromMesh("ogrehead.mesh")
local lightObj = Object:fromLight(Ogre.Light.LT_POINT, Ogre.ColourValue(1, 1, 1))
lightObj:setPosition(Ogre.Vector3(200, 300, 200))
local cam = scene:getCamera("MainCamera")
local st = SphereTrack:new(cam)
st.distance = 200
st.elevation = 0.3
local rw = bbfx.Engine.instance():getRenderWindow()
local vp = rw:getViewport(0)
local tn = bbfx.RootTimeNode.instance()
local animator = bbfx.Animator.instance()
local frameCount = 0

local updateNode = bbfx.LuaAnimationNode("test", function(self)
    local dtPort = self:getInput("dt")
    if not dtPort then return end
    frameCount = frameCount + 1
    headObj.node:yaw(Ogre.Radian(0.01))
    st.azimuth = st.azimuth + 0.003
    local cosEl = math.cos(st.elevation)
    st.camNode:setPosition(Ogre.Vector3(
        st.distance * cosEl * math.cos(st.azimuth),
        st.distance * math.sin(st.elevation),
        st.distance * cosEl * math.sin(st.azimuth)))
    st.camNode:lookAt(Ogre.Vector3(0, 0, 0), 2)

    if frameCount == 30 then
        rw:writeContentsToFile("final_BEFORE.png")
        print("[TEST] BEFORE (frame 30)")
    end
    if frameCount == 60 then
        vp:setClearEveryFrame(false)
        local cmgr = Ogre.CompositorManager.getSingleton()

        -- Test Bloom (natif OGRE, shaders GLSL dans Bloom.material)
        local ok1 = pcall(function() cmgr:addCompositor(vp, "Bloom") end)
        local ok2 = pcall(function() cmgr:setCompositorEnabled(vp, "Bloom", true) end)
        print("[TEST] Bloom: add=" .. tostring(ok1) .. " enable=" .. tostring(ok2))
    end
    if frameCount == 120 then
        rw:writeContentsToFile("final_BLOOM_120.png")
        print("[TEST] BLOOM frame 120")
    end
    if frameCount == 180 then
        rw:writeContentsToFile("final_BLOOM_180.png")
        print("[TEST] BLOOM frame 180")
    end
end)
updateNode:addInput("dt")
animator:addNode(tn)
animator:addNode(updateNode)
animator:addPort(tn, "dt", updateNode, "dt")
