-- helpers.lua — BBFx v2.3 utility functions

-- UID generator for unique scene node names
local uid_counter = 0
function UID(prefix)
    uid_counter = uid_counter + 1
    return (prefix or "uid/") .. uid_counter
end

-- curry function for Note.Action
function curry(f, ...)
    local args = {...}
    return function(...)
        local all = {}
        for _, v in ipairs(args) do all[#all+1] = v end
        for _, v in ipairs({...}) do all[#all+1] = v end
        return f(table.unpack(all))
    end
end

-- Global convenience accessors (set after Engine init)
function bbfx_globals()
    scene = bbfx.Engine.instance():getSceneManager()
    engine = bbfx.Engine.instance()
    renderWindow = engine:getRenderWindow()
    keyboard = bbfx.InputManager.instance():getKeyboard()
end
