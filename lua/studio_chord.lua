-- studio_chord.lua — BBFx v3.1 Studio Chord System
-- Simplified chord activation API for the Studio GUI.
-- Wraps the existing Chord state machine (chord.lua) with a name-based interface.

ChordSystem = {}
ChordSystem.__index = ChordSystem

-- Active chord states (name → bool)
ChordSystem._active = {}

-- All registered chord names
ChordSystem._chords = {}

--- Register a chord name (called by Timeline/Serializer when chords are defined)
function ChordSystem:register(name)
    if not self._chords[name] then
        self._chords[name] = true
        self._active[name] = false
    end
end

--- Activate a chord state by name
function ChordSystem:activate(name)
    self:register(name)
    self._active[name] = true
end

--- Deactivate a chord state by name
function ChordSystem:deactivate(name)
    if self._active[name] ~= nil then
        self._active[name] = false
    end
end

--- Toggle a chord state by name
function ChordSystem:toggle(name)
    self:register(name)
    self._active[name] = not self._active[name]
end

--- Check if a chord is active
function ChordSystem:isActive(name)
    return self._active[name] == true
end

--- List all registered chord names
function ChordSystem:list()
    local names = {}
    for name, _ in pairs(self._chords) do
        table.insert(names, name)
    end
    table.sort(names)
    return names
end

--- List all active chord names
function ChordSystem:getActive()
    local names = {}
    for name, active in pairs(self._active) do
        if active then
            table.insert(names, name)
        end
    end
    return names
end

--- Deactivate all chords (panic)
function ChordSystem:panic()
    for name, _ in pairs(self._active) do
        self._active[name] = false
    end
end
