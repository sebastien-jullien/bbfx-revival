-- joystick_mapping.lua — BBFx v2.3 Joystick Mapping (SDL3)
-- Ported from 2006 production code (prog/workspace/bbfx/bbfx/src/lua/0.1/joystick.lua)

require 'helpers'

Joystick = {
    devices = {},
    bindings = {}
}
Joystick.__index = Joystick

-- Open a joystick by index (0-based)
function Joystick:open(index)
    local o = {}
    setmetatable(o, self)
    o.index = index or 0
    o.joyMgr = bbfx.InputManager.instance():getJoystick()
    o.axisBindings = {}
    o.buttonBindings = {}
    o.buttonState = {}
    Joystick.devices[index] = o
    print("[joystick] Opened joystick " .. index)
    return o
end

-- Lookup joystick by name (iterates connected devices)
function Joystick:lookup(name)
    -- SDL3 gamepad API doesn't expose device names the same way
    -- For now, return the first available joystick
    local joyMgr = bbfx.InputManager.instance():getJoystick()
    if joyMgr:getCount() > 0 then
        print("[joystick] Found joystick (lookup: " .. name .. ")")
        return Joystick:open(0)
    end
    print("[joystick] No joystick found for: " .. name)
    return nil
end

-- Bind a joystick axis to an AnimationPort
function Joystick:bind(type, index, target)
    if type == "axis" then
        self.axisBindings[index] = target
    elseif type == "button" then
        self.buttonBindings[index] = target
    end
end

-- Poll axes and buttons, update bound targets
function Joystick:poll()
    if not self.joyMgr then return end

    -- Update axis bindings
    for axisIndex, target in pairs(self.axisBindings) do
        local value = self.joyMgr:getAxisValue(0, axisIndex)
        -- getAxisValue returns -1..1, remap to 0..1
        local normalized = (value + 1.0) * 0.5
        if type(target) == "function" then
            target(normalized)
        elseif target.setValue then
            target:setValue(normalized)
        end
    end

    -- Update button bindings (trigger on press)
    for btnIndex, callback in pairs(self.buttonBindings) do
        local pressed = self.joyMgr:isButtonDown(0, btnIndex)
        local wasPressed = self.buttonState[btnIndex] or false
        if pressed and not wasPressed then
            if type(callback) == "function" then
                callback()
            end
        end
        self.buttonState[btnIndex] = pressed
    end
end

-- Cleanup
function Joystick:cleanup()
    Joystick.devices = {}
    Joystick.bindings = {}
end
