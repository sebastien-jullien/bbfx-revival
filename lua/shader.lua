-- shader.lua — BBFx v2.8 Shader Loader
-- Loads GLSL shaders and creates ShaderFxNode instances in the DAG

require 'helpers'
require 'logger'

Shader = {}
Shader.__index = Shader

ShaderManager = {}
ShaderManager._registry = {}

function Shader:load(vertPath, params)
    params = params or {}
    local name = UID("shader/")
    local fragPath = params.frag or "shaders/passthrough.frag"
    local entity = params.mesh

    if not entity then
        Logger.error("[Shader] No mesh specified in params")
        return nil
    end

    local node = bbfx.ShaderFxNode(name, vertPath, fragPath, scene, entity)

    -- Set initial uniform values from params
    for k, v in pairs(params) do
        if k ~= "mesh" and k ~= "frag" and type(v) == "number" then
            local port = node:getInput(k)
            if port then
                port:setValue(v)
            end
        end
    end

    -- Register in DAG
    local animator = bbfx.Animator.instance()
    local tn = bbfx.RootTimeNode.instance()
    animator:addNode(tn)
    animator:addNode(node)
    animator:addPort(tn, "dt", node, "dt")

    -- Create wrapper
    local wrapper = {
        _node = node,
        _name = name,
        _vertPath = vertPath,
    }
    setmetatable(wrapper, Shader)

    -- Register in ShaderManager
    ShaderManager._registry[name] = wrapper

    Logger.info("[Shader] Loaded: " .. vertPath .. " as " .. name)
    return wrapper
end

function Shader:setUniform(uname, value)
    local port = self._node:getInput(uname)
    if port then
        port:setValue(value)
    else
        Logger.warn("[Shader] Uniform not found: " .. tostring(uname))
    end
end

function Shader:list()
    print("=== Loaded Shaders ===")
    local count = 0
    for name, s in pairs(ShaderManager._registry) do
        print("  " .. name .. " → " .. s._vertPath)
        count = count + 1
    end
    if count == 0 then print("  (none)") end
    print("(" .. count .. " shaders)")
end

-- ShaderManager accessors
function ShaderManager:get(name)
    return self._registry[name]
end

function ShaderManager:create(name, params)
    return Shader:load(name, params)
end
