-- sol2_compat.lua — Compatibility layer replacing the old SWIG-based swig.lua
-- Provides stubs for scripts that used to require("swig")

local sol2_compat = {}

-- In sol2, types are already native. No wrapping needed.
function sol2_compat.state()
    return nil -- No SWIG state concept in sol2
end

function sol2_compat.wrap(obj)
    return obj -- Objects are already wrapped by sol2
end

function sol2_compat.type(obj)
    return type(obj)
end

return sol2_compat
