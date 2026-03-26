-- animation.lua — BBFx v2.5 Spline Animation System
-- Port of 2006 compo.lua Animation:new/addFrames/create/bind pattern
-- Supports IM_LINEAR and IM_SPLINE (Catmull-Rom) interpolation

require 'helpers'

Animation = {}
Animation.__index = Animation

-- Interpolation mode constants (mirror Ogre::Animation::InterpolationMode)
Animation.IM_LINEAR = 0
Animation.IM_SPLINE = 1

-- Rotation interpolation mode constants
Animation.RIM_LINEAR = 0

-- ============================================================
-- Constructor
-- ============================================================

function Animation:new()
    local o = {
        _frames     = {},
        _cumulative = {},   -- cumulative time at end of each frame
        _total_len  = 0,
        _interp     = Animation.IM_LINEAR,
        _rim        = Animation.RIM_LINEAR,
        _loop       = false,
        _time       = 0.0,
        _playing    = false,
        _dac_node   = nil,  -- internal LuaAnimationNode driving the animation
        _targets    = {},   -- bound SceneNodes
    }
    setmetatable(o, Animation)
    return o
end

-- ============================================================
-- addFrames — accepts varargs of frame tables
-- Each frame: {length=N, translate=Vector3?, rotate=Quaternion?, scale=Vector3?}
-- ============================================================

function Animation:addFrames(...)
    for _, frame in ipairs({...}) do
        local f = {
            length    = frame.length    or 1.0,
            translate = frame.translate or nil,
            rotate    = frame.rotate    or nil,
            scale     = frame.scale     or nil,
        }
        table.insert(self._frames, f)
    end
end

-- ============================================================
-- create — finalise the animation
-- interp_mode : Animation.IM_LINEAR or Animation.IM_SPLINE
-- rim_mode    : Animation.RIM_LINEAR
-- loop        : boolean
-- ============================================================

function Animation:create(interp_mode, rim_mode, loop)
    self._interp = interp_mode or Animation.IM_LINEAR
    self._rim    = rim_mode    or Animation.RIM_LINEAR
    self._loop   = (loop == true)

    -- Precompute cumulative times
    self._cumulative = {}
    local t = 0.0
    for i, frame in ipairs(self._frames) do
        t = t + frame.length
        self._cumulative[i] = t
    end
    self._total_len = t
end

-- ============================================================
-- Internal: Catmull-Rom spline for a single float
-- ============================================================

local function catmull_rom(p0, p1, p2, p3, t)
    local t2 = t * t
    local t3 = t2 * t
    return 0.5 * (
        (2.0 * p1) +
        (-p0 + p2) * t +
        (2.0*p0 - 5.0*p1 + 4.0*p2 - p3) * t2 +
        (-p0 + 3.0*p1 - 3.0*p2 + p3) * t3
    )
end

-- ============================================================
-- Internal: interpolate a Vector3 between two frames
-- For spline, uses 4 surrounding control points
-- ============================================================

local function lerp_vec3(a, b, t)
    return Ogre.Vector3(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    )
end

local function spline_vec3(frames, idx, frac)
    local n = #frames
    local function get_translate(i)
        local f = frames[math.max(1, math.min(i, n))]
        return f.translate or Ogre.Vector3(0, 0, 0)
    end
    local p0 = get_translate(idx - 1)
    local p1 = get_translate(idx)
    local p2 = get_translate(idx + 1)
    local p3 = get_translate(idx + 2)
    return Ogre.Vector3(
        catmull_rom(p0.x, p1.x, p2.x, p3.x, frac),
        catmull_rom(p0.y, p1.y, p2.y, p3.y, frac),
        catmull_rom(p0.z, p1.z, p2.z, p3.z, frac)
    )
end

-- ============================================================
-- Internal: sample the animation at time t, apply to SceneNode
-- ============================================================

function Animation:_sample(node)
    if #self._frames < 1 then return end

    local t = self._time
    local n = #self._frames

    -- Find segment
    local seg_idx = 1
    local seg_start = 0.0
    for i = 1, n do
        if t <= self._cumulative[i] then
            seg_idx = i
            break
        end
        seg_start = self._cumulative[i]
        if i == n then seg_idx = n end
    end

    -- Fraction within segment
    local seg_len = self._cumulative[seg_idx] - seg_start
    local frac = 0.0
    if seg_len > 0.001 then
        frac = math.max(0.0, math.min(1.0, (t - seg_start) / seg_len))
    end

    -- Get current and next frames
    local f_cur  = self._frames[seg_idx]
    local f_next = self._frames[math.min(seg_idx + 1, n)]

    -- Apply translation
    if f_cur.translate then
        local next_t = f_next.translate or f_cur.translate
        local pos
        if self._interp == Animation.IM_SPLINE and n >= 3 then
            pos = spline_vec3(self._frames, seg_idx, frac)
        else
            pos = lerp_vec3(f_cur.translate, next_t, frac)
        end
        node:setPosition(pos)
    end

    -- Apply scale
    if f_cur.scale then
        local next_s = f_next.scale or f_cur.scale
        local s = lerp_vec3(f_cur.scale, next_s, frac)
        node:setScale(s)
    end

    -- Apply rotation (slerp)
    if f_cur.rotate then
        local next_r = f_next.rotate or f_cur.rotate
        local r = Ogre.Quaternion.Slerp(frac, f_cur.rotate, next_r, true)
        node:setOrientation(r)
    end
end

-- ============================================================
-- bind — connect animation to one or more SceneNodes or Objects
-- Supported: Ogre SceneNode userdata, or objects with .animNode (SceneNode)
-- Creates a LuaAnimationNode to drive the animation each frame
-- ============================================================

function Animation:bind(...)
    self._targets = {}
    for _, target in ipairs({...}) do
        -- Accept SceneNode directly or objects with .animNode
        if type(target) == "userdata" then
            table.insert(self._targets, target)
        elseif type(target) == "table" and target.animNode then
            table.insert(self._targets, target.animNode)
        elseif type(target) == "table" and target.node then
            table.insert(self._targets, target.node)
        end
    end

    -- Create the driving LuaAnimationNode (only once)
    if self._dac_node then
        bbfx.Animator.instance():removeNode(self._dac_node)
        self._dac_node = nil
    end

    local anim_ref = self  -- capture
    local node = bbfx.LuaAnimationNode(UID("anim/"), function(self_node)
        if not anim_ref._playing then return end
        local dtPort = self_node:getInput("dt")
        if not dtPort then return end
        local dt = dtPort:getValue()

        -- Advance time
        anim_ref._time = anim_ref._time + dt

        -- Loop or clamp
        if anim_ref._total_len > 0 then
            if anim_ref._loop then
                anim_ref._time = anim_ref._time % anim_ref._total_len
            else
                anim_ref._time = math.min(anim_ref._time, anim_ref._total_len)
            end
        end

        -- Apply to all bound targets
        for _, tgt in ipairs(anim_ref._targets) do
            anim_ref:_sample(tgt)
        end
    end)
    node:addInput("dt")

    local animator = bbfx.Animator.instance()
    animator:addNode(node)

    -- Connect to RootTimeNode dt
    local tn = bbfx.RootTimeNode.instance()
    animator:addNode(tn)
    animator:addPort(tn, "dt", node, "dt")

    self._dac_node = node
    return self
end

-- ============================================================
-- play / stop / setTime / getTime
-- ============================================================

function Animation:play()
    self._playing = true
    self._time = 0.0
    return self
end

function Animation:stop()
    self._playing = false
    return self
end

function Animation:resume()
    self._playing = true
    return self
end

function Animation:setTime(t)
    self._time = t
    return self
end

function Animation:getTime()
    return self._time
end

function Animation:isPlaying()
    return self._playing
end
