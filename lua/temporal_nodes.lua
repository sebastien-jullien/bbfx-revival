-- temporal_nodes.lua — BBFx v2.5 Temporal Animation Nodes
-- DelayNode, RampNode, LFONode, EnvelopeFollowerNode as LuaAnimationNode

require 'helpers'

-- ============================================================
-- LFONode — periodic signal generator
-- Ports: dt(in), frequency(in), amplitude(in), offset(in),
--        waveform(in), phase(in), out(out)
-- ============================================================

LFONode = {}
LFONode.SIN    = 0
LFONode.TRI    = 1
LFONode.SQUARE = 2
LFONode.SAW    = 3

function LFONode:new(params)
    params = params or {}
    local frequency = params.frequency or 1.0
    local amplitude = params.amplitude or 1.0
    local offset    = params.offset    or 0.0
    local waveform  = params.waveform  or LFONode.SIN
    local phase     = params.phase     or 0.0

    local t = 0.0  -- accumulated time (in seconds)

    local node = bbfx.LuaAnimationNode(UID("lfo/"), function(self)
        local dtPort   = self:getInput("dt")
        local freqPort = self:getInput("frequency")
        local ampPort  = self:getInput("amplitude")
        local offPort  = self:getInput("offset")
        local wfPort   = self:getInput("waveform")
        local phPort   = self:getInput("phase")
        local outPort  = self:getOutput("out")
        if not (dtPort and outPort) then return end

        local dt   = dtPort:getValue()
        local freq = freqPort:getValue()
        local amp  = ampPort:getValue()
        local off  = offPort:getValue()
        local wf   = math.floor(wfPort:getValue() + 0.5)
        local ph   = phPort:getValue()

        -- v3.2: beat sync mode
        local bsPort = self:getInput("beat_sync")
        local bfPort = self:getInput("beatFrac")
        local beatSync = bsPort and bsPort:getValue() >= 0.5

        if beatSync and bfPort then
            -- In beat sync mode, frequency is in beats and phase uses beatFrac
            local beatFrac = bfPort:getValue()
            t = t + dt  -- still accumulate for multi-beat periods
            local tp_raw = t * freq + ph
            -- Use beatFrac for sub-beat precision
            local tp = (tp_raw + beatFrac * freq) % 1.0
        else
            t = t + dt
        end
        local tp = (t * freq + ph) % 1.0  -- normalized phase 0..1

        local v
        if wf == LFONode.TRI then
            v = (tp < 0.5) and (tp * 4.0 - 1.0) or (3.0 - tp * 4.0)
        elseif wf == LFONode.SQUARE then
            v = (tp < 0.5) and 1.0 or -1.0
        elseif wf == LFONode.SAW then
            v = tp * 2.0 - 1.0
        else  -- SIN (default)
            v = math.sin(tp * 2.0 * math.pi)
        end

        outPort:setValue(off + amp * v)
    end)

    node:addInput("dt")
    node:addInput("frequency")
    node:addInput("amplitude")
    node:addInput("offset")
    node:addInput("waveform")
    node:addInput("phase")
    node:addInput("beat_sync")
    node:addInput("beatFrac")
    node:addOutput("out")

    -- Set default values on input ports
    node:getInput("frequency"):setValue(frequency)
    node:getInput("amplitude"):setValue(amplitude)
    node:getInput("offset"):setValue(offset)
    node:getInput("waveform"):setValue(waveform)
    node:getInput("phase"):setValue(phase)

    local animator = bbfx.Animator.instance()
    animator:addNode(node)

    -- Return wrapper for external access
    local wrapper = { _node = node }
    setmetatable(wrapper, { __index = function(t, k)
        return rawget(t, k) or LFONode[k]
    end})
    return wrapper
end

-- ============================================================
-- RampNode — linear interpolation toward target at fixed rate
-- Ports: dt(in), target(in), rate(in), out(out)
-- ============================================================

RampNode = {}

function RampNode:new(params)
    params = params or {}
    local rate_default = params.rate or 1.0
    local current = params.initial or 0.0

    local node = bbfx.LuaAnimationNode(UID("ramp/"), function(self)
        local dtPort     = self:getInput("dt")
        local targetPort = self:getInput("target")
        local ratePort   = self:getInput("rate")
        local outPort    = self:getOutput("out")
        if not (dtPort and targetPort and outPort) then return end

        local dt     = dtPort:getValue()
        local target = targetPort:getValue()
        local rate   = ratePort:getValue()

        local diff = target - current
        local step = rate * dt
        if math.abs(diff) <= 0.001 then
            current = target
        elseif diff > 0 then
            current = math.min(current + step, target)
        else
            current = math.max(current - step, target)
        end

        outPort:setValue(current)
    end)

    node:addInput("dt")
    node:addInput("target")
    node:addInput("rate")
    node:addInput("beat_sync")
    node:addOutput("out")

    node:getInput("rate"):setValue(rate_default)
    node:getInput("target"):setValue(current)

    local animator = bbfx.Animator.instance()
    animator:addNode(node)

    local wrapper = { _node = node }
    setmetatable(wrapper, { __index = function(t, k)
        return rawget(t, k) or RampNode[k]
    end})
    return wrapper
end

-- ============================================================
-- DelayNode — circular buffer, delays signal by delay_time seconds
-- Ports: dt(in), in(in), delay_time(in), out(out)
-- Max buffer: 600 entries (~10s at 60fps)
-- ============================================================

DelayNode = {}
local DELAY_MAX_BUFFER = 600

function DelayNode:new(params)
    params = params or {}
    local delay_default = params.delay_time or 0.0

    local buffer = {}    -- ring buffer: {value, timestamp}
    local time_acc = 0.0

    local node = bbfx.LuaAnimationNode(UID("delay/"), function(self)
        local dtPort    = self:getInput("dt")
        local inPort    = self:getInput("in")
        local dPort     = self:getInput("delay_time")
        local outPort   = self:getOutput("out")
        if not (dtPort and inPort and dPort and outPort) then return end

        local dt         = dtPort:getValue()
        local val        = inPort:getValue()
        local delay_time = dPort:getValue()

        time_acc = time_acc + dt

        -- Push new sample
        buffer[#buffer + 1] = {v = val, t = time_acc}

        -- Evict oldest entries beyond buffer capacity
        while #buffer > DELAY_MAX_BUFFER do
            table.remove(buffer, 1)
        end

        -- Find oldest sample that is >= delay_time old
        local target_t = time_acc - delay_time
        local out_val = 0.0
        for i = 1, #buffer do
            if buffer[i].t <= target_t then
                out_val = buffer[i].v
            else
                break
            end
        end

        outPort:setValue(out_val)
    end)

    node:addInput("dt")
    node:addInput("in")
    node:addInput("delay_time")
    node:addOutput("out")

    node:getInput("delay_time"):setValue(delay_default)

    local animator = bbfx.Animator.instance()
    animator:addNode(node)

    local wrapper = { _node = node }
    setmetatable(wrapper, { __index = function(t, k)
        return rawget(t, k) or DelayNode[k]
    end})
    return wrapper
end

-- ============================================================
-- EnvelopeFollowerNode — attack/release envelope
-- Ports: dt(in), in(in, trigger 0..1), attack(in, seconds),
--        release(in, seconds), out(out, 0..1)
-- ============================================================

EnvelopeFollowerNode = {}

function EnvelopeFollowerNode:new(params)
    params = params or {}
    local attack_default  = params.attack  or 0.1
    local release_default = params.release or 0.5

    local current = 0.0

    local node = bbfx.LuaAnimationNode(UID("envelope/"), function(self)
        local dtPort      = self:getInput("dt")
        local inPort      = self:getInput("in")
        local attackPort  = self:getInput("attack")
        local releasePort = self:getInput("release")
        local outPort     = self:getOutput("out")
        if not (dtPort and inPort and attackPort and releasePort and outPort) then return end

        local dt      = dtPort:getValue()
        local trigger = inPort:getValue()
        local attack  = math.max(attackPort:getValue(), 0.001)
        local release = math.max(releasePort:getValue(), 0.001)

        if trigger > current then
            -- Attack phase: rise toward trigger
            local rate = 1.0 / attack
            current = math.min(current + rate * dt, trigger)
        else
            -- Release phase: fall toward 0
            local rate = 1.0 / release
            current = math.max(current - rate * dt, 0.0)
        end

        outPort:setValue(current)
    end)

    node:addInput("dt")
    node:addInput("in")
    node:addInput("attack")
    node:addInput("release")
    node:addOutput("out")

    node:getInput("attack"):setValue(attack_default)
    node:getInput("release"):setValue(release_default)

    local animator = bbfx.Animator.instance()
    animator:addNode(node)

    local wrapper = { _node = node }
    setmetatable(wrapper, { __index = function(t, k)
        return rawget(t, k) or EnvelopeFollowerNode[k]
    end})
    return wrapper
end
