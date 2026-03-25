-- keycodes.lua — SDLK_* → symbolic names for common keys

local Keycodes = {
    ESCAPE = 27,
    SPACE = 32,
    RETURN = 13,
    TAB = 9,
    BACKSPACE = 8,
    DELETE = 127,
    UP = 1073741906,
    DOWN = 1073741905,
    LEFT = 1073741904,
    RIGHT = 1073741903,
    F1 = 1073741882,
    F2 = 1073741883,
    F3 = 1073741884,
    F4 = 1073741885,
    F5 = 1073741886,
    F6 = 1073741887,
    F7 = 1073741888,
    F8 = 1073741889,
    F9 = 1073741890,
    F10 = 1073741891,
    F11 = 1073741892,
    F12 = 1073741893,
    LSHIFT = 1073742049,
    RSHIFT = 1073742053,
    LCTRL = 1073742048,
    RCTRL = 1073742052,
    LALT = 1073742050,
    RALT = 1073742054,
}

-- Add letter keys A-Z
for i = 0, 25 do
    Keycodes[string.char(65 + i)] = 97 + i -- lowercase ASCII
end

-- Add number keys 0-9
for i = 0, 9 do
    Keycodes["NUM" .. i] = 48 + i
end

return Keycodes
