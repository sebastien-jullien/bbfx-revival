-- ogre_controller.lua — BBFx v2.3 Waveform Functions
-- Ported from 2006 production code (pure Lua math, no OGRE dependency)

Waveform = {}

function Waveform.sin(time, frequency)
    frequency = frequency or 1.0
    return math.sin(time * frequency * 2 * math.pi)
end

function Waveform.triangle(time, frequency)
    frequency = frequency or 1.0
    local t = (time * frequency) % 1.0
    if t < 0.5 then
        return t * 4.0 - 1.0
    else
        return 3.0 - t * 4.0
    end
end

function Waveform.square(time, frequency)
    frequency = frequency or 1.0
    local t = (time * frequency) % 1.0
    if t < 0.5 then return 1.0 else return -1.0 end
end

function Waveform.sawtooth(time, frequency)
    frequency = frequency or 1.0
    local t = (time * frequency) % 1.0
    return t * 2.0 - 1.0
end
