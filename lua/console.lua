-- console.lua — BBFx v2.6 REPL Console
-- LuaConsoleNode reads stdin non-blocking and evaluates Lua expressions

require 'helpers'
require 'errorhandler'

-- Create StdinReader singleton
local _stdin = bbfx.StdinReader()

-- ── Introspection commands (global functions) ──────────────────────────

function graph()
    local animator = bbfx.Animator.instance()
    local nodes = animator:getNodeNames()
    if nodes then
        print("=== DAG Nodes ===")
        for i, name in ipairs(nodes) do
            print("  " .. name)
        end
        print("(" .. #nodes .. " nodes)")
    else
        print("(no node listing available — getNodeNames not bound)")
    end
end

function ports(node_name)
    local animator = bbfx.Animator.instance()
    local node = animator:getNodeByName(node_name)
    if not node then
        print("Node not found: " .. tostring(node_name))
        return
    end
    local inputs = node:getInputNames()
    local outputs = node:getOutputNames()
    print("=== " .. node_name .. " ===")
    if inputs then
        print("  Inputs:")
        for _, name in ipairs(inputs) do
            local port = node:getInput(name)
            local val = port and port:getValue() or "?"
            print("    " .. name .. " = " .. tostring(val))
        end
    end
    if outputs then
        print("  Outputs:")
        for _, name in ipairs(outputs) do
            local port = node:getOutput(name)
            local val = port and port:getValue() or "?"
            print("    " .. name .. " = " .. tostring(val))
        end
    end
end

function set(node_name, port_name, value)
    local animator = bbfx.Animator.instance()
    local node = animator:getNodeByName(node_name)
    if not node then
        print("Node not found: " .. tostring(node_name))
        return
    end
    local port = node:getInput(port_name) or node:getOutput(port_name)
    if not port then
        print("Port not found: " .. tostring(port_name) .. " on " .. node_name)
        return
    end
    port:setValue(value)
    print(node_name .. "." .. port_name .. " = " .. tostring(value))
end

function reload(script)
    print("[console] Reloading: " .. tostring(script))
    ErrorHandler.dofile(script)
end

function help()
    print("=== BBFx Console Commands ===")
    print("  graph()                    — list all DAG nodes")
    print("  ports('nodeName')          — list ports of a node")
    print("  set('node', 'port', val)   — set a port value")
    print("  reload('script.lua')       — reload a Lua script")
    print("  watch('script.lua')        — add to hot reload watch list")
    print("  unwatch('script.lua')      — remove from watch list")
    print("  watchlist()                — show watched files")
    print("  audio()                    — show audio capture status")
    print("  hud()                      — toggle HUD overlay")
    print("  help()                     — show this help")
    print("  quit()                     — stop the engine")
    print("  <any Lua expression>       — evaluate and print result")
end

function audio()
    if not _G._bbfx_audio then
        print("[audio] No audio instance active. Use Audio:start() first.")
        return
    end
    local a = _G._bbfx_audio
    print("=== Audio Status ===")
    print("  Running: " .. tostring(a:isRunning()))
    print("  RMS:     " .. string.format("%.4f", a:getRMS()))
    print("  Peak:    " .. string.format("%.4f", a:getPeak()))
    print("  BPM:     " .. string.format("%.1f", a:getBPM()))
    print("  Low:     " .. string.format("%.4f", a:getBand("low")))
    print("  Mid:     " .. string.format("%.4f", a:getBand("mid")))
    print("  High:    " .. string.format("%.4f", a:getBand("high")))
end

function quit()
    print("[console] Shutting down...")
    bbfx.Engine.instance():stopRendering()
end

-- ── LuaConsoleNode ─────────────────────────────────────────────────────

local consoleNode = bbfx.LuaAnimationNode(UID("console/"), function(self)
    local line = _stdin:poll()
    if line then
        if line ~= "" then
            local ok, result = ErrorHandler.eval(line)
            if ok and result ~= nil then
                print(tostring(result))
            end
        end
        io.write("bbfx> ")
        io.flush()
    end
end)
consoleNode:addInput("dt")

local animator = bbfx.Animator.instance()
local tn = bbfx.RootTimeNode.instance()
animator:addNode(tn)
animator:addNode(consoleNode)
animator:addPort(tn, "dt", consoleNode, "dt")

-- Initial prompt
io.write("bbfx> ")
io.flush()
