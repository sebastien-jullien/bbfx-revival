-- declarative.lua — BBFx v2.5 Declarative Graph Builder
-- build(graph_table) constructs an animation DAG from a Lua table description
--
-- Format:
--   build({
--     nodes = {
--       {name="lfo",    type="LFO",      frequency=1.0, amplitude=0.5},
--       {name="ramp",   type="Ramp",     rate=2.0},
--       {name="delay",  type="Delay",    delay_time=0.5},
--       {name="env",    type="Envelope", attack=0.1, release=0.5},
--       {name="fx",     type="existing", ref=perlinFxNode},
--       {name="pp",     type="preset",   preset="PerlinPulse", frequency=1.0},
--     },
--     links = {
--       {from="lfo.out", to="ramp.target"},
--       {from="ramp.out", to="fx.displacement"},
--     }
--   })
-- Returns: {nodes = {name -> node_instance}}

require 'helpers'
require 'temporal_nodes'
require 'subgraph'

-- Node type resolvers
local function resolve_node(spec)
    local t = spec.type
    if t == "LFO" then
        return LFONode:new({
            frequency = spec.frequency or 1.0,
            amplitude = spec.amplitude or 1.0,
            offset    = spec.offset    or 0.0,
            waveform  = spec.waveform  or LFONode.SIN,
            phase     = spec.phase     or 0.0,
        })
    elseif t == "Ramp" then
        return RampNode:new({
            rate    = spec.rate    or 1.0,
            initial = spec.initial or 0.0,
        })
    elseif t == "Delay" then
        return DelayNode:new({
            delay_time = spec.delay_time or 0.0,
        })
    elseif t == "Envelope" then
        return EnvelopeFollowerNode:new({
            attack  = spec.attack  or 0.1,
            release = spec.release or 0.5,
        })
    elseif t == "existing" then
        assert(spec.ref, "declarative: type='existing' requires a 'ref' field")
        return spec.ref
    elseif t == "preset" then
        assert(spec.preset, "declarative: type='preset' requires a 'preset' field")
        return Preset:instantiate(spec.preset, spec)
    else
        error("declarative: unknown node type: " .. tostring(t))
    end
end

-- Get the raw bbfx node from a wrapper or direct node
local function raw_node(n)
    if type(n) == "table" then
        -- SubgraphNode: no single raw node; caller must use SubgraphNode:getNode()
        if n._node then return n._node end
        return n
    end
    return n
end

-- Build function
function build(graph_table)
    assert(type(graph_table) == "table", "build() requires a table")
    local nodes_spec = graph_table.nodes or {}
    local links_spec = graph_table.links or {}

    -- Step 1: instantiate all nodes
    local handles = {}  -- name -> node_instance
    for _, spec in ipairs(nodes_spec) do
        assert(spec.name, "declarative: each node must have a 'name' field")
        handles[spec.name] = resolve_node(spec)
    end

    -- Step 2: wire links
    local animator = bbfx.Animator.instance()
    for _, link in ipairs(links_spec) do
        local function parse_spec(s)
            local nname, pname = s:match("^(.+)%.(.+)$")
            assert(nname and pname, "declarative link bad spec: " .. tostring(s))
            return nname, pname
        end

        local from_nname, from_pname = parse_spec(link.from)
        local to_nname,   to_pname   = parse_spec(link.to)

        local from_node_wrap = handles[from_nname]
        local to_node_wrap   = handles[to_nname]
        assert(from_node_wrap, "declarative: unknown node '" .. from_nname .. "' in link")
        assert(to_node_wrap,   "declarative: unknown node '" .. to_nname   .. "' in link")

        local from_raw = raw_node(from_node_wrap)
        local to_raw   = raw_node(to_node_wrap)

        -- For SubgraphNode, resolve through exposed ports
        if type(from_node_wrap) == "table" and from_node_wrap._outputs and from_node_wrap._outputs[from_pname] then
            local mapping = from_node_wrap._outputs[from_pname]
            local inner = from_node_wrap._nodes[mapping.node]
            from_raw = raw_node(inner)
            from_pname = mapping.port
        end
        if type(to_node_wrap) == "table" and to_node_wrap._inputs and to_node_wrap._inputs[to_pname] then
            local mapping = to_node_wrap._inputs[to_pname]
            local inner = to_node_wrap._nodes[mapping.node]
            to_raw = raw_node(inner)
            to_pname = mapping.port
        end

        animator:addPort(from_raw, from_pname, to_raw, to_pname)
    end

    return { nodes = handles }
end
