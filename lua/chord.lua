-- chord.lua — BBFx v2.3 Chord State Machine
-- Ported from 2006 production code (prog/workspace/bbfx/bbfx/src/lua/0.1/chord.lua)

Chord = {}
Chord.__index = Chord

function Chord:new(o)
    o = o or {}
    setmetatable(o, self)
    return o
end

-- Create a new chord with empty sequence
function Chord:create()
    local chord = Chord:new()
    chord.sequence = {}
    return chord
end

-- Add a chord state
function Chord:addState(index)
    self.sequence[index] = {}
end

-- Return all notes for a state
function Chord:getNotes(index)
    local set = {}
    if self.sequence[index] then
        for _, v in pairs(self.sequence[index]) do
            for _, w in pairs(v) do
                set[w.note] = true
            end
        end
    end
    return set
end

-- Define a note within a state
-- @param index: state index
-- @param note: Note object
-- @param start: beat offset within the state
-- @param duration: duration in beats
-- @param speed: playback speed
function Chord:defState(index, note, start, duration, speed)
    speed = speed or 1.0
    duration = duration or 1
    local seq = {note = note, duration = duration, speed = speed}
    self.sequence[index][start] = self.sequence[index][start] or {}
    table.insert(self.sequence[index][start], seq)
end

-- Send a chord state to the sequencer
-- @param index: state index
-- @param sync: Sync object
-- @param pos: position offset
-- @param flag: "beat", "bar", or "cycle"
function Chord:send(index, sync, pos, flag)
    flag = flag or "beat"
    if not self.sequence[index] then
        print("[chord] Warning: state " .. index .. " not defined")
        return
    end
    if flag == "beat" then
        sync.sequencer:schedule(self.sequence[index], pos)
    elseif flag == "bar" and sync.map then
        local beatPos = sync.map.beat[pos] or pos
        sync.sequencer:schedule(self.sequence[index], beatPos)
    elseif flag == "cycle" and sync.map then
        local barPos = sync.map.bar[pos] or pos
        local beatPos = sync.map.beat[barPos] or barPos
        sync.sequencer:schedule(self.sequence[index], beatPos)
    end
end
