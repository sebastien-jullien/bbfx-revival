local engine = bbfx.Engine.instance()
local scene = engine:getSceneManager()
scene:setAmbientLight(Ogre.ColourValue(0.5,0.5,0.5))
local spin = scene:getRootSceneNode():createChildSceneNode("S")
local elapsed = 0
local tn = bbfx.RootTimeNode.instance()
local animator = bbfx.Animator.instance()
local w = bbfx.LuaAnimationNode("w", function(self)
    local p = self:getInput("dt")
    if p then elapsed = elapsed + p:getValue() end
    spin:yaw(Ogre.Radian(0.01))
    if elapsed >= 60 then
        print("[longrun] 60 seconds elapsed — stopping engine.")
        engine:stopRendering()
    end
end)
w:addInput("dt")
animator:addNode(tn)
animator:addNode(w)
animator:addPort(tn, "dt", w, "dt")
print("[longrun] Starting 60-second stability test...")
