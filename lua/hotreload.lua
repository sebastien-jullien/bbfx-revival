-- hotreload.lua — BBFx v2.6 Hot Reload System
-- Watches Lua files and reloads them when modified

require 'helpers'
require 'errorhandler'
require 'logger'

local _watchList = {}  -- path -> {modTime=number}
local _checkInterval = 1.0  -- seconds between checks
local _timeSinceCheck = 0.0

-- Global commands
function watch(path)
    local t = bbfx.fileModTime(path)
    if t == 0 then
        Logger.warn("[HotReload] File not found: " .. tostring(path))
        return
    end
    _watchList[path] = {modTime = t}
    Logger.info("[HotReload] Watching: " .. path)
end

function unwatch(path)
    if _watchList[path] then
        _watchList[path] = nil
        Logger.info("[HotReload] Unwatched: " .. path)
    else
        Logger.warn("[HotReload] Not watching: " .. tostring(path))
    end
end

function watchlist()
    local count = 0
    print("=== HotReload Watch List ===")
    for path, info in pairs(_watchList) do
        print("  " .. path .. "  (modTime=" .. tostring(info.modTime) .. ")")
        count = count + 1
    end
    if count == 0 then
        print("  (empty)")
    end
    print("(" .. count .. " files)")
end

-- Internal: check for modifications
local function checkFiles(dt)
    _timeSinceCheck = _timeSinceCheck + dt
    if _timeSinceCheck < _checkInterval then return end
    _timeSinceCheck = 0.0

    for path, info in pairs(_watchList) do
        local t = bbfx.fileModTime(path)
        if t > 0 and t ~= info.modTime then
            info.modTime = t
            Logger.info("[RELOAD] " .. path)
            ErrorHandler.dofile(path)
        end
    end
end

-- LuaAnimationNode for frame loop integration
local reloadNode = bbfx.LuaAnimationNode(UID("hotreload/"), function(self)
    local dtPort = self:getInput("dt")
    if not dtPort then return end
    checkFiles(dtPort:getValue())
end)
reloadNode:addInput("dt")

local animator = bbfx.Animator.instance()
local tn = bbfx.RootTimeNode.instance()
animator:addNode(tn)
animator:addNode(reloadNode)
animator:addPort(tn, "dt", reloadNode, "dt")
