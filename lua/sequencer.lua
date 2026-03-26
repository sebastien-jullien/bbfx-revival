-- sequencer.lua — BBFx v2.3 Beat-Based Sequencer (Pure Lua)
-- Ported from 2006 production code, adapted to use LuaAnimationNode instead of C++ bbfx.Sequencer

require 'helpers'

Sequencer = {
    map = {}
}
Sequencer.__index = Sequencer

-- Create a new sequencer
-- @param song: table with {bpm = N, bar = N, cycle = N, ...}
-- @param stepCallback: function called at each beat
function Sequencer:new(song, stepCallback)
    local o = {}
    setmetatable(o, self)
    o.song = song
    o.bpm = song.bpm or 120
    o.beatDuration = 60.0 / o.bpm -- seconds per beat
    o.currentBeat = 0
    o.accumTime = 0.0
    o.stepCallback = stepCallback
    o.scheduledNotes = {} -- {beat = N, callback = function}
    o.sequence = {}

    -- Create a LuaAnimationNode that drives the beat
    o.beatNode = bbfx.LuaAnimationNode(UID("sequencer/"), function(self)
        local dtPort = self:getInput("dt")
        if not dtPort then return end
        local dt = dtPort:getValue()

        o.accumTime = o.accumTime + dt
        while o.accumTime >= o.beatDuration do
            o.accumTime = o.accumTime - o.beatDuration
            o.currentBeat = o.currentBeat + 1

            -- Execute scheduled note events for this beat
            local toRemove = {}
            for i, event in ipairs(o.scheduledNotes) do
                if event.beat <= o.currentBeat then
                    event.callback()
                    toRemove[#toRemove + 1] = i
                end
            end
            -- Remove executed events (reverse order)
            for i = #toRemove, 1, -1 do
                table.remove(o.scheduledNotes, toRemove[i])
            end

            -- Call step callback
            if o.stepCallback then
                o.stepCallback()
            end
        end
    end)
    o.beatNode:addInput("dt")

    -- Register in DAG
    local animator = bbfx.Animator.instance()
    local tn = bbfx.RootTimeNode.instance()
    animator:addNode(o.beatNode)
    animator:addPort(tn, "dt", o.beatNode, "dt")

    Sequencer.map[song] = o
    return o
end

-- Schedule a note on/off at specific beats
function Sequencer:scheduleNote(time, notes)
    for _, noteEntry in pairs(notes) do
        local note = noteEntry.note
        local duration = noteEntry.duration or 1
        local speed = noteEntry.speed or 1.0

        -- Schedule note ON
        table.insert(self.scheduledNotes, {
            beat = self.currentBeat + time,
            callback = function() note:on(speed) end
        })

        -- Schedule note OFF
        table.insert(self.scheduledNotes, {
            beat = self.currentBeat + time + duration,
            callback = function() note:off() end
        })
    end
end

-- Schedule a full sequence at an offset
function Sequencer:schedule(sequence, pos)
    pos = pos or 0
    for time, notes in pairs(sequence) do
        self:scheduleNote(time + pos, notes)
    end
end

-- Get current beat
function Sequencer:getTime()
    return self.currentBeat
end

-- Delete
function Sequencer:delete()
    Sequencer.map[self.song] = nil
end
