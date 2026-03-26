-- object.lua — BBFx v2.3 Scene Object Factory
-- Ported from 2006 production code (prog/workspace/bbfx/bbfx/src/lua/0.1/object.lua)

require 'helpers'

Object = {
    map = {}
}
Object.__index = Object

-- Base constructor. Creates a new void movable with its dedicated node.
function Object:new()
    local o = {}
    setmetatable(o, self)
    local name = UID("node/")
    local node = scene:getRootSceneNode():createChildSceneNode(name)
    local childName = name .. "child/"
    local child = node:createChildSceneNode(childName)
    o.node = node
    o.animNode = child
    o.name = name
    Object.map[name] = o
    return o
end

function Object:delete()
    if self.name then
        Object.map[self.name] = nil
    end
end

function Object:cleanup()
    for _, o in pairs(self.map) do o:delete() end
end

-- Create object from mesh file
function Object:fromMesh(meshFile, material)
    local o = Object:new()
    local entityName = UID("entity/")
    local entity = scene:createEntity(entityName, meshFile)
    if material then
        entity:setMaterialName(material)
    end
    o.movable = entity
    o.animNode:attachObject(entity)
    return o
end

-- Create object from billboard set
function Object:fromBillboard(material, w, h)
    local o = Object:new()
    w = w or 10
    h = h or 10
    local bbsName = UID("bbs/")
    local bbs = scene:createBillboardSet(bbsName, 1)
    bbs:setDefaultDimensions(w, h)
    if material then
        bbs:setMaterialName(material)
    end
    bbs:createBillboard(Ogre.Vector3(0, 0, 0))
    o.movable = bbs
    o.animNode:attachObject(bbs)
    return o
end

-- Create object from light
function Object:fromLight(kind, diff, spec)
    local o = Object:new()
    local lightName = UID("light/")
    local light = scene:createLight(lightName)
    if kind then light:setType(kind) end
    if diff then light:setDiffuseColour(diff) end
    if spec then light:setSpecularColour(spec) end
    o.movable = light
    o.animNode:attachObject(light)
    return o
end

-- Create object from particle system template
function Object:fromPsys(psysName)
    local o = Object:new()
    local name = UID("psys/")
    local psys = Ogre.createParticleSystem(scene, name, psysName)
    o.movable = psys
    o.animNode:attachObject(psys)
    return o
end

-- Create object wrapping an existing camera
function Object:fromCamera(camera)
    local o = Object:new()
    o.movable = camera
    o.animNode:attachObject(camera)
    return o
end

-- Create floor plane
function Object:fromFloorPlane(material)
    local o = Object:new()
    local planeName = UID("plane/")
    local plane = Ogre.Plane(Ogre.Vector3(0, 1, 0), 0)
    local meshMgr = Ogre.MeshManager.getSingleton()
    meshMgr:createPlane(planeName, "General", plane, 1000, 1000, 10, 10, true)
    local entity = scene:createEntity(UID("entity/"), planeName)
    if material then
        entity:setMaterialName(material)
    end
    o.movable = entity
    o.animNode:attachObject(entity)
    return o
end

-- Attach movable to scene
function Object:attach()
    if self.movable and self.animNode then
        -- Already attached in fromXxx, this is a re-attach after detach
    end
    if self.node then
        self.node:setVisible(true)
    end
end

-- Detach movable from scene
function Object:detach()
    if self.node then
        self.node:setVisible(false)
    end
end

-- Transform helpers
function Object:translate(v)
    self.node:translate(v)
end

function Object:translateX(val)
    self.node:translate(Ogre.Vector3(val, 0, 0))
end

function Object:translateY(val)
    self.node:translate(Ogre.Vector3(0, val, 0))
end

function Object:translateZ(val)
    self.node:translate(Ogre.Vector3(0, 0, val))
end

function Object:rotate(q)
    self.node:setOrientation(q)
end

function Object:rotateX(angle)
    self.node:pitch(Ogre.Radian(angle))
end

function Object:rotateY(angle)
    self.node:yaw(Ogre.Radian(angle))
end

function Object:rotateZ(angle)
    self.node:roll(Ogre.Radian(angle))
end

function Object:scale(v)
    self.node:setScale(v)
end

function Object:scaleX(val)
    local s = self.node:getScale()
    self.node:setScale(Ogre.Vector3(val, s.y, s.z))
end

function Object:scaleY(val)
    local s = self.node:getScale()
    self.node:setScale(Ogre.Vector3(s.x, val, s.z))
end

function Object:scaleZ(val)
    local s = self.node:getScale()
    self.node:setScale(Ogre.Vector3(s.x, s.y, val))
end

function Object:setPosition(v)
    self.node:setPosition(v)
end
