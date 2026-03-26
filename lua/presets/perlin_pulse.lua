-- presets/perlin_pulse.lua — BBFx v2.5
-- Preset "PerlinPulse": LFONode (sin) → RampNode → PerlinFxNode
-- Exposed inputs: frequency, amplitude, rate, displacement

require 'subgraph'
require 'temporal_nodes'

Preset:define("PerlinPulse", function(args)
    args = args or {}
    local frequency   = args.frequency   or 0.5
    local amplitude   = args.amplitude   or 30.0
    local rate        = args.rate        or 2.0
    local perlin_node = args.perlin_node or nil  -- optional: existing PerlinFxNode

    local sg = SubgraphNode:new("PerlinPulse")

    -- Internal nodes
    local lfo  = LFONode:new({frequency=frequency, amplitude=amplitude, waveform=LFONode.SIN})
    local ramp = RampNode:new({rate=rate})

    sg:addNode(lfo,  "lfo")
    sg:addNode(ramp, "ramp")

    -- Internal links: lfo.out → ramp.target
    sg:link("lfo.out", "ramp.target")

    -- If a PerlinFxNode is provided, wire ramp.out → perlin.displacement
    if perlin_node then
        local animator = bbfx.Animator.instance()
        local ramp_raw = ramp._node
        local perlin_raw = (type(perlin_node) == "table" and perlin_node._node) or perlin_node
        animator:addPort(ramp_raw, "out", perlin_raw, "displacement")
        sg:addNode(perlin_node, "perlin")
    end

    -- Expose external interface
    sg:exposeInput("frequency",   "lfo",  "frequency")
    sg:exposeInput("amplitude",   "lfo",  "amplitude")
    sg:exposeInput("rate",        "ramp", "rate")
    sg:exposeOutput("value",      "ramp", "out")

    return sg
end)
