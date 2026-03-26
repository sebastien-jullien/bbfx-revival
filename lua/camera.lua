-- camera.lua — BBFx v2.3 Camera + SphereTrack
-- Ported from 2006 production code + SphereTrack extension

Camera = {}

-- Setup a camera with position and lookAt
function Camera.setup(cam, pos, target, nearClip, fov)
    local camNode = scene:getRootSceneNode():createChildSceneNode(UID("cam/"))
    camNode:attachObject(cam)
    if pos then camNode:setPosition(pos) end
    if target then camNode:lookAt(target, 2) end -- TS_WORLD
    if nearClip then cam:setNearClipDistance(nearClip) end
    if fov then cam:setFOVy(fov) end
    cam:setAutoAspectRatio(true)
    return camNode
end

-- SphereTrack: orbital camera controlled by azimuth, elevation, distance
-- Creates a LuaAnimationNode with input ports for control
SphereTrack = {}

function SphereTrack:new(cam, target)
    local st = {}
    st.cam = cam
    st.target = target or Ogre.Vector3(0, 0, 0)
    st.azimuth = 0.0
    st.elevation = 0.3
    st.distance = 200.0
    st.camNode = cam:getParentSceneNode() or scene:getRootSceneNode():createChildSceneNode(UID("stcam/"))

    -- Create AnimationNode for DAG integration
    st.animNode = bbfx.LuaAnimationNode("SphereTrack", function(self)
        local dtPort = self:getInput("dt")
        if not dtPort then return end
        local dt = dtPort:getValue()

        -- Read control inputs if connected
        local azPort = self:getInput("azimuth_delta")
        local elPort = self:getInput("elevation_delta")
        local distPort = self:getInput("distance_delta")

        if azPort then st.azimuth = st.azimuth + azPort:getValue() * dt end
        if elPort then st.elevation = st.elevation + elPort:getValue() * dt end
        if distPort then st.distance = st.distance + distPort:getValue() * dt end

        -- Clamp elevation
        if st.elevation > 1.5 then st.elevation = 1.5 end
        if st.elevation < -1.5 then st.elevation = -1.5 end
        if st.distance < 10 then st.distance = 10 end

        -- Spherical to cartesian
        local cosEl = math.cos(st.elevation)
        local x = st.distance * cosEl * math.cos(st.azimuth)
        local y = st.distance * math.sin(st.elevation)
        local z = st.distance * cosEl * math.sin(st.azimuth)

        st.camNode:setPosition(Ogre.Vector3(x + st.target.x, y + st.target.y, z + st.target.z))
        st.camNode:lookAt(st.target, 2) -- TS_WORLD
    end)
    st.animNode:addInput("dt")
    st.animNode:addInput("azimuth_delta")
    st.animNode:addInput("elevation_delta")
    st.animNode:addInput("distance_delta")

    setmetatable(st, {__index = SphereTrack})
    return st
end

-- Auto-rotate (default behavior when no joystick)
function SphereTrack:autoRotate(speed)
    speed = speed or 0.5
    self.autoSpeed = speed
end
