-- shell/server.lua — BBFx v2.6 TCP Shell Server
-- Port of 2006 production server.lua via bbfx.TcpServer C++ backend

require 'helpers'
require 'errorhandler'
require 'logger'

ShellServer = {}
ShellServer.__index = ShellServer

Config = Config or {}
Config.shell = Config.shell or {host = "127.0.0.1", port = 33195, max_clients = 2}

function ShellServer:new(config)
    config = config or Config.shell
    local port = config.port or 33195
    local max_clients = config.max_clients or 2

    local o = {}
    setmetatable(o, ShellServer)
    o._server = bbfx.TcpServer(port, max_clients)
    o._server:start()

    -- Create LuaAnimationNode to poll each frame
    o._node = bbfx.LuaAnimationNode(UID("shell/"), function(self_node)
        o:_poll()
    end)
    o._node:addInput("dt")

    local animator = bbfx.Animator.instance()
    local tn = bbfx.RootTimeNode.instance()
    animator:addNode(tn)
    animator:addNode(o._node)
    animator:addPort(tn, "dt", o._node, "dt")

    Logger.info("[ShellServer] TCP server started on port " .. port)
    return o
end

function ShellServer:_poll()
    if not self._server:isRunning() then return end
    local messages = self._server:pollRaw()
    for _, msg in ipairs(messages) do
        local clientId = msg.id
        local expr = msg.text
        if expr ~= "" then
            Logger.info("[ShellServer] Client " .. clientId .. ": " .. expr)
            local ok, result = ErrorHandler.eval(expr)
            if ok then
                self._server:send(clientId, "--> " .. tostring(result))
            else
                self._server:send(clientId, "error: " .. tostring(result))
            end
        end
    end
end

function ShellServer:stop()
    if self._server then
        self._server:stop()
        Logger.info("[ShellServer] Stopped")
    end
end
