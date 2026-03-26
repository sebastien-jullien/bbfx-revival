-- sync.lua — BBFx v2.3 Time Synchronization
-- Ported from 2006 production code (prog/workspace/bbfx/bbfx/src/lua/0.1/sync.lua)

require 'sequencer'

Sync = {}

-- Create a new synchronized sequencer
-- @param song: {bpm = 120, bar = 4, cycle = 16, {bar=4, cycle=4}, ...}
function Sync:new(song)
    local o = {}
    setmetatable(o, {__index = Sync})
    o.sequencer = Sequencer:new(song, function() o:step() end)
    o.beat = 0
    o.nextBar = 1
    o.nextCycle = 1
    o.song = song
    o.map = o:createMap()
    return o
end

-- Schedule a chord state event
function Sync:scheduleState(flag, chord, state)
    local time = self.sequencer:getTime()
    if flag == "beat" then
        chord:send(state, self, time + 1)
    elseif flag == "bar" then
        if self.map.beat[self.nextBar] then
            chord:send(state, self, self.map.beat[self.nextBar])
        end
    elseif flag == "cycle" then
        local barPos = self.map.bar[self.nextCycle]
        if barPos and self.map.beat[barPos] then
            chord:send(state, self, self.map.beat[barPos])
        end
    end
end

-- Schedule a single note
function Sync:scheduleNote(flag, note, duration, speed)
    local time = self.sequencer:getTime()
    local tblNote = {{note = note, duration = duration, speed = speed}}
    if flag == "beat" then
        self.sequencer:scheduleNote(time + 1, tblNote)
    elseif flag == "bar" then
        if self.map.beat[self.nextBar] then
            self.sequencer:scheduleNote(self.map.beat[self.nextBar], tblNote)
        end
    elseif flag == "cycle" then
        local barPos = self.map.bar[self.nextCycle]
        if barPos and self.map.beat[barPos] then
            self.sequencer:scheduleNote(self.map.beat[barPos], tblNote)
        end
    end
end

-- Create the song mapping (beat → bar → cycle)
function Sync:createMap()
    local map = {beat = {}, bar = {}}
    local beat = 0
    local bar = 1
    local rootBar = self.song.bar or 4
    local rootCycle = self.song.cycle or 16

    for i, part in ipairs(self.song) do
        local partBar = part.bar or rootBar
        local partCycle = part.cycle or rootCycle
        for iBar = 1, partBar do
            table.insert(map.beat, beat)
            beat = beat + partBar
        end
        bar = bar + partCycle
        table.insert(map.bar, bar)
    end
    return map
end

-- Beat step callback
function Sync:step()
    self.beat = self.beat + 1
    if self.map.beat[self.nextBar] then
        if self.beat > self.map.beat[self.nextBar] then
            self.nextBar = self.nextBar + 1
        end
    end
    if self.map.bar[self.nextCycle] then
        if self.nextBar > self.map.bar[self.nextCycle] then
            self.nextCycle = self.nextCycle + 1
        end
    end
end

-- v2.7: Auto-mode — use BPM from audio analysis
function Sync:setAutoMode(audio)
    self._audioSource = audio
    if audio then
        Logger.info("[Sync] Auto BPM mode enabled")
    else
        Logger.info("[Sync] Auto BPM mode disabled — using fixed BPM")
    end
end

function Sync:getEffectiveBPM()
    if self._audioSource and self._audioSource:isRunning() then
        local bpm = self._audioSource:getBPM()
        if bpm > 0 then return bpm end
    end
    return self.song.bpm
end
