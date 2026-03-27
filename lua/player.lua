-- player.lua — BBFx v2.9 Input Player (Replay)
-- Replays a .bbfx-session file, injecting events into the DAG

require 'helpers'
require 'logger'

local _player = nil

function replay(filename)
    if _player and _player:isPlaying() then
        Logger.warn("[player] Already replaying. Use stopreplay() first.")
        return
    end
    _player = bbfx.InputPlayer()
    _player:play(filename or "session.bbfx-session")

    -- Create update node to advance replay and dispatch events
    local node = bbfx.LuaAnimationNode(UID("player/"), function(self)
        local dtPort = self:getInput("dt")
        if not dtPort or not _player or not _player:isPlaying() then return end
        local dt = dtPort:getValue()
        local events = _player:getNextEvents(dt)
        for _, ev in ipairs(events) do
            if ev.type == "key" then
                -- Could inject into keyboard node if needed
                Logger.info("[replay] Key " .. ev.code .. " " .. ev.state .. " at t=" .. string.format("%.3f", ev.time))
            elseif ev.type == "axis" then
                Logger.info("[replay] Axis " .. ev.code .. " = " .. string.format("%.3f", ev.value) .. " at t=" .. string.format("%.3f", ev.time))
            elseif ev.type == "beat" then
                Logger.info("[replay] Beat at t=" .. string.format("%.3f", ev.time))
            end
        end
    end)
    node:addInput("dt")
    local animator = bbfx.Animator.instance()
    local tn = bbfx.RootTimeNode.instance()
    animator:addNode(node)
    animator:addPort(tn, "dt", node, "dt")

    Logger.info("[player] Replay started: " .. (filename or "session.bbfx-session"))
end

function stopreplay()
    if _player and _player:isPlaying() then
        _player:stop()
        Logger.info("[player] Replay stopped")
    else
        Logger.warn("[player] Not replaying")
    end
end
