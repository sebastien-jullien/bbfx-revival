-- remdebug.lua — BBFx v2.9 Remote Debug Integration
-- Wraps mobdebug for remote Lua debugging via VS Code

require 'logger'

function debug_connect(host, port)
    host = host or "127.0.0.1"
    port = port or 8172

    local ok, mobdebug = pcall(require, "lib/mobdebug")
    if not ok then
        Logger.warn("[remdebug] mobdebug not found in lua/lib/. Download from https://github.com/pkulchenko/MobDebug")
        Logger.info("[remdebug] Place mobdebug.lua in lua/lib/mobdebug.lua")
        return false
    end

    Logger.info("[remdebug] Connecting to " .. host .. ":" .. port .. "...")
    local success, err = pcall(function()
        mobdebug.start(host, port)
    end)

    if success then
        Logger.info("[remdebug] Connected — breakpoints active")
        return true
    else
        Logger.warn("[remdebug] Connection failed: " .. tostring(err))
        Logger.info("[remdebug] Make sure VS Code lua-debug extension is listening on port " .. port)
        return false
    end
end

-- REPL command
function debug()
    debug_connect()
end
