-- compo.lua — BBFx v2.5
-- Port of 2006 production compo.lua (BonneBalleFx Compositions)
-- Adaptations:
--   Ogre.Animation.IM_SPLINE -> Animation.IM_SPLINE (v2.5 constant)
--   Dictionary:proxy() -> stubbed (controller-based reflection not ported)
--   bbfx.LuaControllerValue / createValueAnimation -> stubbed
--   Particle templates: wrapped in pcall (may not exist in resources)

require 'helpers'
require 'object'
require 'animation'
require 'sync'
require 'note'
require 'chord'

-- Stub: Dictionary proxy — not ported in v2.5
Dictionary = Dictionary or {}
function Dictionary.proxy(movable)
    print("WARNING: Dictionary:proxy() not available in v2.5 — stub")
    return setmetatable({}, {__index = function() return function() end end, __newindex = function() end})
end

Compo = {}

-- Helper: safely create a particle system, returns nil if template missing
local function safe_psys(name)
    local ok, result = pcall(function() return Object:fromPsys(name) end)
    if ok then return result else
        print("WARNING: particle template '" .. name .. "' not found — skipping")
        return nil
    end
end

-- Helper: bind animation to object's animNode (skip if object is nil)
local function safe_bind(anim, obj)
    if not obj then return nil end
    return anim:bind(obj.animNode)
end

-- ============================================================
-- Compo.starwars — Port of 2006 "BonneBalleFx first composition"
-- ============================================================

function Compo.starwars()
    local song = {bpm = 144, bar = 4, cycle = 4}
    for i = 1, 32 do song[i] = {bar = 4, cycle = 4} end
    local sync = Sync:new(song)

    -- Particle systems
    local psys_1  = safe_psys("particle/starwars")
    local psys_2  = safe_psys("particle/starwars1")
    if psys_2  then psys_2:translateZ(200) end
    local psys_3  = safe_psys("particle/starwars1")
    if psys_3  then psys_3:translateZ(200) end
    local psys_4  = safe_psys("PEExamples/strupx")
    if psys_4  then psys_4:translateZ(200) end
    local psys_5  = safe_psys("PEExamples/stop")
    if psys_5  then psys_5:translateZ(200) end
    local psys_6  = safe_psys("PEExamples/the")
    if psys_6  then psys_6:translateZ(200) end
    local psys_7  = safe_psys("PEExamples/gun")
    if psys_7  then psys_7:translateZ(200) end
    local psys_8  = safe_psys("PEExamples/stop")
    if psys_8  then psys_8:translateZ(200) psys_8:translateY(50) end
    local psys_9  = safe_psys("PEExamples/the")
    if psys_9  then psys_9:translateZ(200) end
    local psys_10 = safe_psys("PEExamples/gun")
    if psys_10 then psys_10:translateZ(200) psys_10:translateY(-50) end

    -- Animations (port: Ogre.Animation.IM_SPLINE -> Animation.IM_SPLINE)
    local anim_1 = Animation:new()
    anim_1:addFrames(
        {length = 1, translate = Ogre.Vector3(0, 0, 0)},
        {length = 1, translate = Ogre.Vector3(0, 0, 100)}
    )
    anim_1:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)

    local anim_2 = Animation:new()
    anim_2:addFrames(
        {length = 1, translate = Ogre.Vector3(0, 0, 0)},
        {length = 1, translate = Ogre.Vector3(0, 0, 100)},
        {length = 1, translate = Ogre.Vector3(0, 0, 150)},
        {length = 1, translate = Ogre.Vector3(0, 0, 100)}
    )
    anim_2:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)

    local anim_3 = Animation:new()
    anim_3:addFrames(
        {length = 1, translate = Ogre.Vector3(0, 0, 0)},
        {length = 1, translate = Ogre.Vector3(0, 0, 130)},
        {length = 1, translate = Ogre.Vector3(0, 0, 160)},
        {length = 1, translate = Ogre.Vector3(0, 0, 130)}
    )
    anim_3:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)

    local anim_4 = Animation:new()
    anim_4:addFrames(
        {length = 1, rotate = Ogre.Quaternion.IDENTITY},
        {length = 1, rotate = Ogre.Quaternion(0, 0, 1, 0)}
    )
    anim_4:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)

    local anim_5 = Animation:new()
    anim_5:addFrames(
        {length = 1, rotate = Ogre.Quaternion.IDENTITY},
        {length = 1, rotate = Ogre.Quaternion(0, 0, 1, 0)}
    )
    anim_5:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)

    local anim_6 = Animation:new()
    anim_6:addFrames(
        {length = 1, translate = Ogre.Vector3(0, 0, 0)},
        {length = 1, translate = Ogre.Vector3(0, 0, 100)},
        {length = 1, translate = Ogre.Vector3(0, 0, 150)},
        {length = 1, translate = Ogre.Vector3(0, 0, 100)}
    )
    anim_6:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)

    local anim_8  = Animation:new()
    anim_8:addFrames(
        {length = 1, rotate = Ogre.Quaternion.IDENTITY},
        {length = 1, rotate = Ogre.Quaternion(0, 0, 1, 0)}
    )
    anim_8:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)

    local anim_9  = Animation:new()
    anim_9:addFrames(
        {length = 1, rotate = Ogre.Quaternion.IDENTITY},
        {length = 1, rotate = Ogre.Quaternion(0, 0, 1, 0)}
    )
    anim_9:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)

    local anim_10 = Animation:new()
    anim_10:addFrames(
        {length = 1, rotate = Ogre.Quaternion.IDENTITY},
        {length = 1, rotate = Ogre.Quaternion(0, 0, 1, 0)}
    )
    anim_10:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)

    local anim_11 = Animation:new()
    anim_11:addFrames(
        {length = 1, rotate = Ogre.Quaternion.IDENTITY},
        {length = 1, rotate = Ogre.Quaternion(0, 0, 1, 0)}
    )
    anim_11:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)

    local anim_12 = Animation:new()
    anim_12:addFrames(
        {length = 1, rotate = Ogre.Quaternion.IDENTITY},
        {length = 1, rotate = Ogre.Quaternion(0, 0, 1, 0)}
    )
    anim_12:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)

    local anim_13 = Animation:new()
    anim_13:addFrames(
        {length = 1, rotate = Ogre.Quaternion.IDENTITY},
        {length = 1, rotate = Ogre.Quaternion(0, 0, 1, 0)}
    )
    anim_13:create(Animation.IM_SPLINE, Animation.RIM_LINEAR, true)

    -- Note: animValue/animValue2 (LuaControllerValue-based) are not ported in v2.5
    -- note_30 and note_31 are stubbed (nil = no-op in defState)

    -- Notes
    local note_1  = psys_1  and Note:fromObject(psys_1)  or nil
    local note_2  = psys_2  and Note:fromObject(psys_2)  or nil
    local note_3  = safe_bind(anim_1,  psys_1)  and Note:fromAnimation(safe_bind(anim_1,  psys_1)) or nil
    local note_4  = safe_bind(anim_2,  psys_2)  and Note:fromAnimation(safe_bind(anim_2,  psys_2)) or nil
    local note_5  = safe_bind(anim_3,  psys_3)  and Note:fromAnimation(safe_bind(anim_3,  psys_3)) or nil
    local note_6  = psys_3  and Note:fromObject(psys_3)  or nil
    local note_8  = psys_4  and Note:fromObject(psys_4)  or nil
    local note_9  = safe_bind(anim_5,  psys_4)  and Note:fromAnimation(safe_bind(anim_5,  psys_4)) or nil
    local note_10 = safe_bind(anim_6,  psys_4)  and Note:fromAnimation(safe_bind(anim_6,  psys_4)) or nil
    local note_11 = psys_5  and Note:fromObject(psys_5)  or nil
    local note_12 = psys_6  and Note:fromObject(psys_6)  or nil
    local note_13 = psys_7  and Note:fromObject(psys_7)  or nil
    local note_14 = psys_8  and Note:fromObject(psys_8)  or nil
    local note_15 = psys_9  and Note:fromObject(psys_9)  or nil
    local note_16 = psys_10 and Note:fromObject(psys_10) or nil
    local note_22 = safe_bind(anim_8,  psys_5)  and Note:fromAnimation(safe_bind(anim_8,  psys_5)) or nil
    local note_23 = safe_bind(anim_9,  psys_6)  and Note:fromAnimation(safe_bind(anim_9,  psys_6)) or nil
    local note_24 = safe_bind(anim_10, psys_7)  and Note:fromAnimation(safe_bind(anim_10, psys_7)) or nil
    local note_25 = safe_bind(anim_11, psys_8)  and Note:fromAnimation(safe_bind(anim_11, psys_8)) or nil
    local note_26 = safe_bind(anim_12, psys_9)  and Note:fromAnimation(safe_bind(anim_12, psys_9)) or nil
    local note_27 = safe_bind(anim_13, psys_10) and Note:fromAnimation(safe_bind(anim_13, psys_10)) or nil

    -- Chord (safe defState: skip nil notes)
    local chord = Chord:create()
    local function safe_def(state, note, ...)
        if note then chord:defState(state, note, ...) end
    end

    chord:addState(1)
    safe_def(1, note_1,  1,  128)
    safe_def(1, note_2,  1,  128)
    safe_def(1, note_3,  1,  3,  1/2) safe_def(1, note_3,  4, 3, 1/2)
    safe_def(1, note_3,  8,  3,  1/2) safe_def(1, note_3, 12, 3, 1/2)
    safe_def(1, note_3,  16, 3,  1/2) safe_def(1, note_3, 20, 3, 1/2)
    safe_def(1, note_3,  24, 3,  1/2) safe_def(1, note_3, 28, 3, 1/2)
    safe_def(1, note_3,  32, 3,  1/2) safe_def(1, note_3, 36, 3, 1/2)
    safe_def(1, note_3,  40, 3,  1/2) safe_def(1, note_3, 44, 3, 1/2)
    safe_def(1, note_3,  48, 3,  1/2) safe_def(1, note_3, 52, 3, 1/2)
    safe_def(1, note_3,  56, 3,  1/2) safe_def(1, note_3, 60, 3, 1/2)
    safe_def(1, note_4,  1,  128, 1/32)
    safe_def(1, note_5,  8,  8,  1/8) safe_def(1, note_5, 24, 8, 1/8)
    safe_def(1, note_5,  40, 8,  1/8) safe_def(1, note_5, 56, 8, 1/8)
    safe_def(1, note_6,  1,  128)

    chord:addState(2)
    safe_def(2, note_11, 4,   8) safe_def(2, note_12, 12, 8) safe_def(2, note_13, 20, 8)
    safe_def(2, note_22, 8,   4, 1/4) safe_def(2, note_23, 16, 4, 1/4) safe_def(2, note_24, 24, 4, 1/4)
    safe_def(2, note_11, 32,  4) safe_def(2, note_12, 36, 4) safe_def(2, note_13, 40, 4)
    safe_def(2, note_22, 34,  2, 1/2) safe_def(2, note_23, 38, 2, 1/2) safe_def(2, note_24, 42, 2, 1/2)
    safe_def(2, note_11, 48,  2) safe_def(2, note_12, 50, 2) safe_def(2, note_13, 52, 2)
    safe_def(2, note_11, 56,  1) safe_def(2, note_12, 57, 1) safe_def(2, note_13, 58, 1)
    safe_def(2, note_11, 64,  1) safe_def(2, note_12, 65, 1) safe_def(2, note_13, 66, 1)
    safe_def(2, note_11, 67,  1) safe_def(2, note_12, 68, 1) safe_def(2, note_13, 69, 1)
    safe_def(2, note_11, 70,  1) safe_def(2, note_12, 71, 1) safe_def(2, note_13, 72, 1)
    safe_def(2, note_11, 73,  1) safe_def(2, note_12, 74, 1) safe_def(2, note_13, 75, 1)
    safe_def(2, note_14, 80,  4) safe_def(2, note_15, 80, 4) safe_def(2, note_16, 80, 4)
    safe_def(2, note_25, 82,  4, 1/4) safe_def(2, note_26, 82, 4, 1/4) safe_def(2, note_27, 82, 4, 1/4)
    safe_def(2, note_14, 84,  4) safe_def(2, note_15, 84, 4) safe_def(2, note_16, 84, 4)
    safe_def(2, note_14, 90,  1) safe_def(2, note_15, 90, 1) safe_def(2, note_16, 90, 1)
    safe_def(2, note_14, 92,  1) safe_def(2, note_15, 92, 1) safe_def(2, note_16, 92, 1)
    safe_def(2, note_14, 94,  1) safe_def(2, note_15, 94, 1) safe_def(2, note_16, 94, 1)
    safe_def(2, note_8,  94, 30)
    safe_def(2, note_9,  96,  8, 1/8) safe_def(2, note_9, 106, 4, 1/4)
    safe_def(2, note_9,  112, 2, 1/2) safe_def(2, note_9, 115, 1, 1)
    safe_def(2, note_9,  117, 1, 1)   safe_def(2, note_9, 119, 1, 1)
    safe_def(2, note_9,  121, 8, 1/8)

    chord:send(1, sync, 4)
    chord:send(2, sync, 4)

    sync.sequencer.beat:start()
    print("[Compo.starwars] Beat started at " .. song.bpm .. " BPM")

    return {sync=sync, chord=chord}
end

-- Stubs for incomplete compositions
function Compo.Intro()
    print("[Compo.Intro] stub — not fully ported")
end

function Compo.Finish()
    print("[Compo.Finish] stub — not fully ported")
end

function Compo.lastUse()
    print("[Compo.lastUse] stub — not fully ported")
end
