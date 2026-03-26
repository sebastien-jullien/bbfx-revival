-- audio.lua — BBFx v2.7 Audio Reactive System
-- Wraps AudioCapture → AudioAnalyzer → BeatDetector + BandSplit

require 'helpers'
require 'logger'

Audio = {}
Audio.__index = Audio

-- BandSplitNode: LuaAnimationNode that aggregates frequency bands
-- band_0..2 → low, band_3..5 → mid, band_6..7 → high
-- With exponential smoothing

local SMOOTH = 0.3  -- smoothing factor (0 = instant, 1 = no change)

function Audio:start(params)
    params = params or {}
    local sampleRate = params.sampleRate or 44100
    local bufferSize = params.bufferSize or 2048

    local o = {}
    setmetatable(o, Audio)

    -- Create C++ audio chain
    o._capture = bbfx.AudioCapture(sampleRate, bufferSize)
    local started = o._capture:start()
    if not started then
        Logger.warn("[Audio] No microphone available — running in silent mode")
    end

    o._captureNode = bbfx.AudioCaptureNode("audio_capture", o._capture)
    o._analyzerNode = bbfx.AudioAnalyzerNode("audio_analyzer", o._captureNode)
    o._beatNode = bbfx.BeatDetectorNode("audio_beat", o._analyzerNode)

    -- Register in DAG
    local animator = bbfx.Animator.instance()
    local tn = bbfx.RootTimeNode.instance()
    animator:addNode(tn)
    animator:addNode(o._captureNode)
    animator:addNode(o._analyzerNode)
    animator:addNode(o._beatNode)

    -- Wire dt to beat detector
    animator:addPort(tn, "dt", o._beatNode, "dt")

    -- BandSplit as LuaAnimationNode
    local low, mid, high = 0, 0, 0
    o._bandNode = bbfx.LuaAnimationNode(UID("bandsplit/"), function(self)
        local b0 = o._analyzerNode:getBand(0)
        local b1 = o._analyzerNode:getBand(1)
        local b2 = o._analyzerNode:getBand(2)
        local b3 = o._analyzerNode:getBand(3)
        local b4 = o._analyzerNode:getBand(4)
        local b5 = o._analyzerNode:getBand(5)
        local b6 = o._analyzerNode:getBand(6)
        local b7 = o._analyzerNode:getBand(7)

        local rawLow  = (b0 + b1 + b2) / 3.0
        local rawMid  = (b3 + b4 + b5) / 3.0
        local rawHigh = (b6 + b7) / 2.0

        -- Exponential smoothing
        low  = low  * SMOOTH + rawLow  * (1.0 - SMOOTH)
        mid  = mid  * SMOOTH + rawMid  * (1.0 - SMOOTH)
        high = high * SMOOTH + rawHigh * (1.0 - SMOOTH)

        self:getOutput("low"):setValue(low)
        self:getOutput("mid"):setValue(mid)
        self:getOutput("high"):setValue(high)
    end)
    o._bandNode:addOutput("low")
    o._bandNode:addOutput("mid")
    o._bandNode:addOutput("high")
    animator:addNode(o._bandNode)

    -- Store references
    o.capture = o._capture
    o.analyzer = o._analyzerNode
    o.beat = o._beatNode
    o.bands = o._bandNode

    Logger.info("[Audio] Chain started: Capture → Analyzer → BeatDetector + BandSplit")
    return o
end

function Audio:getRMS()
    return self._analyzerNode:getRMS()
end

function Audio:getPeak()
    return self._analyzerNode:getPeak()
end

function Audio:getBPM()
    local port = self._beatNode:getOutput("bpm")
    return port and port:getValue() or 0
end

function Audio:getBand(name)
    if name == "low" then
        return self._bandNode:getOutput("low"):getValue()
    elseif name == "mid" then
        return self._bandNode:getOutput("mid"):getValue()
    elseif name == "high" then
        return self._bandNode:getOutput("high"):getValue()
    end
    return 0
end

function Audio:stop()
    if self._capture then
        self._capture:stop()
        Logger.info("[Audio] Stopped")
    end
end

function Audio:isRunning()
    return self._capture and self._capture:isRunning()
end
