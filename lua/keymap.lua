-- keymap.lua — BBFx v2.3 Keyboard Hotkey Mapping (SDL3)
-- Ported from 2006 production code (CEGUI hotkeys → SDL3 wasKeyPressed)

require 'helpers'

Keymap = {
    bindings = {}
}

-- Register a key binding
-- keycode: SDL3 keycode (from keycodes.lua or raw number)
-- callback: function to call when key is pressed
function Keymap.register(keycode, callback)
    Keymap.bindings[keycode] = callback
end

-- Poll all registered bindings
function Keymap.poll()
    if not keyboard then return end
    for keycode, callback in pairs(Keymap.bindings) do
        if keyboard:wasKeyPressed(keycode) then
            callback()
        end
    end
end

-- Convenience: register F-keys for chord state switching
function Keymap.registerChordStates(chord, sync, keys)
    keys = keys or {
        [1073741882] = 1, -- F1
        [1073741883] = 2, -- F2
        [1073741884] = 3, -- F3 (conflicts with stats, use F4+)
        [1073741885] = 4, -- F4
        [1073741886] = 5, -- F5
        [1073741887] = 6, -- F6
        [1073741888] = 7, -- F7
    }
    for keycode, stateIndex in pairs(keys) do
        Keymap.register(keycode, function()
            print("[keymap] Chord state: " .. stateIndex)
            chord:send(stateIndex, sync, 0, "beat")
        end)
    end
end

-- Clear all bindings
function Keymap.clear()
    Keymap.bindings = {}
end
