-- textureset.lua — BBFx v2.4 Texture Control
-- Ported from 2006 production code (prog/workspace/bbfx/bbfx/src/lua/0.1/textureset.lua)
-- Adapted: C++ controller values replaced with pure Lua AnimationNode implementations

require 'helpers'

-- TextureControl: manages texture scroll and rotation
TextureControl = {}
TextureControl.__index = TextureControl

function TextureControl:new(material, layerIndex)
    local o = {}
    setmetatable(o, self)
    o.material = material
    o.layer = layerIndex or 0
    o.scrollU = 0
    o.scrollV = 0
    o.rotation = 0
    return o
end

function TextureControl:setScroll(u, v)
    self.scrollU = u or self.scrollU
    self.scrollV = v or self.scrollV
end

function TextureControl:setRotation(angle)
    self.rotation = angle
end

function TextureControl:update(dt)
    -- Pure Lua: texture animation is controlled by the material's texture unit state
    -- In a real implementation, this would call material:getTechnique(0):getPass(0):getTextureUnitState(layer):setTextureScroll(u, v)
    -- For now, accumulate values for use by the animation system
    self.scrollU = self.scrollU
    self.scrollV = self.scrollV
end

-- SweepControl: state machine for texture sweep animation
SweepControl = {}
SweepControl.__index = SweepControl

function SweepControl:new()
    local o = {}
    setmetatable(o, self)
    o.state = "idle" -- idle, sweeping, paused
    o.speed = 1.0
    o.position = 0.0
    o.target = 1.0
    return o
end

function SweepControl:start(target, speed)
    self.target = target or 1.0
    self.speed = speed or 1.0
    self.state = "sweeping"
end

function SweepControl:stop()
    self.state = "idle"
end

function SweepControl:pause()
    self.state = "paused"
end

function SweepControl:update(dt)
    if self.state == "sweeping" then
        if self.position < self.target then
            self.position = math.min(self.position + self.speed * dt, self.target)
        elseif self.position > self.target then
            self.position = math.max(self.position - self.speed * dt, self.target)
        else
            self.state = "idle"
        end
    end
    return self.position
end

-- TextureCycle: rotate between texture presets
TextureCycle = {}
TextureCycle.__index = TextureCycle

function TextureCycle:new(textures)
    local o = {}
    setmetatable(o, self)
    o.textures = textures or {}
    o.currentIndex = 1
    return o
end

function TextureCycle:next()
    self.currentIndex = (self.currentIndex % #self.textures) + 1
    return self.textures[self.currentIndex]
end

function TextureCycle:prev()
    self.currentIndex = ((self.currentIndex - 2) % #self.textures) + 1
    return self.textures[self.currentIndex]
end

function TextureCycle:current()
    return self.textures[self.currentIndex]
end

-- TextureSet: composite texture manager
TextureSet = {}
TextureSet.__index = TextureSet

-- TextureSet:new(settings, cycle)
-- settings: optional table {joystick, uscroll, vscroll, scrollbutton, rspeed, sweep, vsweep, sweepbutton, sweepstate}
-- cycle: optional TextureCycle to bind on creation
function TextureSet:new(settings, cycle)
    local o = {}
    setmetatable(o, self)
    o.controls = {}
    o.cycles = {}
    o.settings = settings or {}
    o.active = false
    o._swap = nil
    if cycle then
        o.cycles["default"] = cycle
    end
    return o
end

function TextureSet:on()
    self.active = true
end

function TextureSet:off()
    self.active = false
end

-- setSwap: link another TextureSet as swap partner
-- A joystick button press (settings.scrollbutton) swaps active state between self and partner
function TextureSet:setSwap(other)
    self._swap = other
    other._swap = self
end

function TextureSet:addControl(name, material, layer)
    local ctrl = TextureControl:new(material, layer)
    self.controls[name] = ctrl
    return ctrl
end

function TextureSet:addCycle(name, textures)
    local cycle = TextureCycle:new(textures)
    self.cycles[name] = cycle
    return cycle
end

function TextureSet:update(dt)
    for _, ctrl in pairs(self.controls) do
        ctrl:update(dt)
    end
end
