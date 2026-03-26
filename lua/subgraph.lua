-- subgraph.lua — BBFx v2.5 Subgraph & Preset System
-- SubgraphNode: encapsulates a group of nodes behind a named port interface
-- Preset: named factory system with save/load

require 'helpers'

-- ============================================================
-- SubgraphNode
-- ============================================================

SubgraphNode = {}
SubgraphNode.__index = SubgraphNode

function SubgraphNode:new(name)
    local o = {
        _name        = name or UID("subgraph/"),
        _nodes       = {},   -- name -> node_instance
        _links       = {},   -- {from_node, from_port, to_node, to_port}
        _inputs      = {},   -- ext_name -> {inner_node_name, inner_port_name}
        _outputs     = {},   -- ext_name -> {inner_node_name, inner_port_name}
    }
    setmetatable(o, SubgraphNode)
    return o
end

-- Add an internal node under a name
function SubgraphNode:addNode(node, name)
    assert(name, "SubgraphNode:addNode requires a name")
    self._nodes[name] = node
    return self
end

-- Link two internal nodes: "node_a.port_x" -> "node_b.port_y"
function SubgraphNode:link(from_spec, to_spec)
    local function parse(spec)
        local node_name, port_name = spec:match("^(.+)%.(.+)$")
        assert(node_name and port_name, "SubgraphNode:link bad spec: " .. tostring(spec))
        return node_name, port_name
    end
    local fn, fp = parse(from_spec)
    local tn, tp = parse(to_spec)
    table.insert(self._links, {fn=fn, fp=fp, tn=tn, tp=tp})

    -- Apply immediately if both nodes exist and have an Animator
    local from_node = self._nodes[fn]
    local to_node   = self._nodes[tn]
    if from_node and to_node then
        local from_n = (type(from_node) == "table" and from_node._node) or from_node
        local to_n   = (type(to_node)   == "table" and to_node._node)   or to_node
        local animator = bbfx.Animator.instance()
        animator:addPort(from_n, fp, to_n, tp)
    end
    return self
end

-- Expose an internal port as an external INPUT of this subgraph
function SubgraphNode:exposeInput(ext_name, inner_node_name, inner_port_name)
    self._inputs[ext_name] = {node = inner_node_name, port = inner_port_name}
    return self
end

-- Expose an internal port as an external OUTPUT of this subgraph
function SubgraphNode:exposeOutput(ext_name, inner_node_name, inner_port_name)
    self._outputs[ext_name] = {node = inner_node_name, port = inner_port_name}
    return self
end

-- Set a value on an exposed input
function SubgraphNode:setInput(ext_name, value)
    local mapping = self._inputs[ext_name]
    if not mapping then
        print("SubgraphNode:setInput — unknown input: " .. tostring(ext_name))
        return self
    end
    local n = self._nodes[mapping.node]
    if not n then
        print("SubgraphNode:setInput — node not found: " .. tostring(mapping.node))
        return self
    end
    local raw = (type(n) == "table" and n._node) or n
    local port = raw:getInput(mapping.port) or raw:getOutput(mapping.port)
    if port then
        port:setValue(value)
    end
    return self
end

-- Get a value from an exposed output
function SubgraphNode:getOutput(ext_name)
    local mapping = self._outputs[ext_name]
    if not mapping then return nil end
    local n = self._nodes[mapping.node]
    if not n then return nil end
    local raw = (type(n) == "table" and n._node) or n
    local port = raw:getOutput(mapping.port)
    if port then return port:getValue() end
    return nil
end

-- Return the internal raw node by name
function SubgraphNode:getNode(name)
    return self._nodes[name]
end

-- ============================================================
-- Preset
-- ============================================================

Preset = {}
Preset._registry = {}

-- Register a named preset factory
-- factory_fn(args) -> SubgraphNode
function Preset:define(name, factory_fn)
    assert(type(name) == "string", "Preset:define name must be string")
    assert(type(factory_fn) == "function", "Preset:define factory_fn must be function")
    Preset._registry[name] = factory_fn
    return self
end

-- Instantiate a preset by name with optional args table
function Preset:instantiate(name, args)
    local factory = Preset._registry[name]
    if not factory then
        error("Preset:instantiate — unknown preset: " .. tostring(name))
    end
    return factory(args or {})
end

-- List all registered preset names
function Preset:list()
    local names = {}
    for k, _ in pairs(Preset._registry) do
        table.insert(names, k)
    end
    table.sort(names)
    return names
end

-- Save exposed input values of a SubgraphNode to a file
-- Format: one "key=value" per line
function Preset:save(sg, filename)
    assert(type(sg) == "table" and sg._inputs, "Preset:save requires a SubgraphNode")
    assert(type(filename) == "string", "Preset:save requires a filename")

    -- Create presets directory if absent (io.open creates the file but not the directory)
    local dir = filename:match("^(.+)[/\\][^/\\]+$")
    if dir then
        -- Portable: try mkdir (Unix) then mkdir (Windows), ignore errors
        os.execute('mkdir -p "' .. dir .. '" 2>/dev/null || mkdir "' .. dir:gsub("/", "\\") .. '" 2>NUL')
    end

    local f, err = io.open(filename, "w")
    if not f then
        print("Preset:save error: " .. tostring(err))
        return false
    end

    for ext_name, mapping in pairs(sg._inputs) do
        local n = sg._nodes[mapping.node]
        if n then
            local raw = (type(n) == "table" and n._node) or n
            local port = raw:getInput(mapping.port)
            if port then
                f:write(ext_name .. "=" .. tostring(port:getValue()) .. "\n")
            end
        end
    end
    f:close()
    return true
end

-- Load preset values from file, returns {params table}
function Preset:load(filename)
    local f, err = io.open(filename, "r")
    if not f then
        print("Preset:load error: " .. tostring(err))
        return nil
    end
    local params = {}
    for line in f:lines() do
        local k, v = line:match("^([^=]+)=(.+)$")
        if k and v then
            -- Try to convert to number
            local num = tonumber(v)
            params[k] = num or v
        end
    end
    f:close()
    return params
end
