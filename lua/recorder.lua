-- recorder.lua — BBFx v2.9 Input Recorder
-- Records keyboard/joystick/audio events to .bbfx-session files

require 'helpers'
require 'logger'

local _recorder = nil

function record(filename)
    if _recorder and _recorder:isRecording() then
        Logger.warn("[recorder] Already recording. Use stoprecord() first.")
        return
    end
    _recorder = bbfx.InputRecorder()
    _recorder:start(filename or "session.bbfx-session")

    -- Create update node to advance time
    local node = bbfx.LuaAnimationNode(UID("recorder/"), function(self)
        local dtPort = self:getInput("dt")
        if dtPort and _recorder and _recorder:isRecording() then
            _recorder:advanceTime(dtPort:getValue())
        end
    end)
    node:addInput("dt")
    local animator = bbfx.Animator.instance()
    local tn = bbfx.RootTimeNode.instance()
    animator:addNode(node)
    animator:addPort(tn, "dt", node, "dt")

    Logger.info("[recorder] Recording started: " .. (filename or "session.bbfx-session"))
end

function stoprecord()
    if _recorder and _recorder:isRecording() then
        _recorder:stop()
        Logger.info("[recorder] Recording stopped")
    else
        Logger.warn("[recorder] Not recording")
    end
end

-- Access the recorder instance for manual event recording
function getRecorder()
    return _recorder
end
