-- controller.lua — BBFx v2.3 Value Controller (AnimationNode-based)
-- Ported from 2006 production code, adapted to v2.2 AnimationNode pattern

require 'helpers'

Controller = {}

-- Linear mapping: output = min + input * (max - min)
function Controller.linear(inputPort, outputPort, min, max)
    min = min or 0.0
    max = max or 1.0
    local node = bbfx.LuaAnimationNode(UID("ctrl_linear/"), function(self)
        local inp = self:getInput("in")
        local out = self:getOutput("out")
        if inp and out then
            local v = inp:getValue()
            out:setValue(min + v * (max - min))
        end
    end)
    node:addInput("in")
    node:addOutput("out")

    -- Connect ports
    local animator = bbfx.Animator.instance()
    animator:addNode(node)
    if inputPort then
        animator:addPort(inputPort:getNode(), inputPort:getName(), node, "in")
    end

    return node
end

-- Smooth mapping: exponential smoothing
function Controller.smooth(inputPort, outputPort, factor)
    factor = factor or 0.1
    local currentVal = 0.0
    local node = bbfx.LuaAnimationNode(UID("ctrl_smooth/"), function(self)
        local inp = self:getInput("in")
        local out = self:getOutput("out")
        if inp and out then
            local target = inp:getValue()
            currentVal = currentVal + (target - currentVal) * factor
            out:setValue(currentVal)
        end
    end)
    node:addInput("in")
    node:addOutput("out")

    local animator = bbfx.Animator.instance()
    animator:addNode(node)

    return node
end

-- Slide mapping: ramp towards target at fixed speed
function Controller.slide(inputPort, outputPort, min, max, speed)
    min = min or 0.0
    max = max or 1.0
    speed = speed or 1.0
    local currentVal = min
    local node = bbfx.LuaAnimationNode(UID("ctrl_slide/"), function(self)
        local dtPort = self:getInput("dt")
        local inp = self:getInput("in")
        local out = self:getOutput("out")
        if inp and out and dtPort then
            local dt = dtPort:getValue()
            local target = inp:getValue() * (max - min) + min
            if currentVal < target then
                currentVal = math.min(currentVal + speed * dt, target)
            elseif currentVal > target then
                currentVal = math.max(currentVal - speed * dt, target)
            end
            out:setValue(currentVal)
        end
    end)
    node:addInput("dt")
    node:addInput("in")
    node:addOutput("out")

    local animator = bbfx.Animator.instance()
    animator:addNode(node)

    return node
end
