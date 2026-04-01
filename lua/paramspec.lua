-- paramspec.lua — Parameter specification system for BBFx presets and nodes
-- Provides typed parameter declarations with constraints and metadata.
-- Used by presets (v2 format) and custom LuaAnimationNodes.

local ParamSpec = {}

--- Create a float parameter definition.
function ParamSpec.float(name, default, opts)
    opts = opts or {}
    return {
        name = name,
        type = "float",
        default = default or 0.0,
        min = opts.min or 0.0,
        max = opts.max or 10.0,
        step = opts.step or 0.01,
        label = opts.label or name
    }
end

--- Create an int parameter definition.
function ParamSpec.int(name, default, opts)
    opts = opts or {}
    return {
        name = name,
        type = "int",
        default = default or 0,
        min = opts.min or 0,
        max = opts.max or 100,
        label = opts.label or name
    }
end

--- Create a bool parameter definition.
function ParamSpec.bool(name, default, opts)
    opts = opts or {}
    return {
        name = name,
        type = "bool",
        default = default or false,
        label = opts.label or name
    }
end

--- Create a string parameter definition.
function ParamSpec.string(name, default, opts)
    opts = opts or {}
    return {
        name = name,
        type = "string",
        default = default or "",
        label = opts.label or name
    }
end

--- Create an enum parameter definition.
function ParamSpec.enum(name, default, choices, opts)
    opts = opts or {}
    return {
        name = name,
        type = "enum",
        default = default or (choices and choices[1] or ""),
        choices = choices or {},
        label = opts.label or name
    }
end

--- Create a color parameter definition (RGB or RGBA).
function ParamSpec.color(name, default, opts)
    opts = opts or {}
    default = default or {1, 1, 1}
    return {
        name = name,
        type = "color",
        default = default,
        label = opts.label or name
    }
end

--- Create a vec3 parameter definition.
function ParamSpec.vec3(name, default, opts)
    opts = opts or {}
    default = default or {0, 0, 0}
    return {
        name = name,
        type = "vec3",
        default = default,
        label = opts.label or name
    }
end

--- Create a mesh parameter definition.
function ParamSpec.mesh(name, default, opts)
    opts = opts or {}
    return {
        name = name,
        type = "mesh",
        default = default or "geosphere4500.mesh",
        label = opts.label or name
    }
end

--- Create a texture parameter definition.
function ParamSpec.texture(name, default, opts)
    opts = opts or {}
    return {
        name = name,
        type = "texture",
        default = default or "",
        label = opts.label or name
    }
end

--- Create a material parameter definition.
function ParamSpec.material(name, default, opts)
    opts = opts or {}
    return {
        name = name,
        type = "material",
        default = default or "BaseWhiteNoLighting",
        label = opts.label or name
    }
end

--- Create a shader parameter definition.
function ParamSpec.shader(name, default, opts)
    opts = opts or {}
    return {
        name = name,
        type = "shader",
        default = default or "",
        label = opts.label or name
    }
end

--- Create a particle template parameter definition.
function ParamSpec.particle(name, default, opts)
    opts = opts or {}
    return {
        name = name,
        type = "particle",
        default = default or "",
        label = opts.label or name
    }
end

--- Create a compositor parameter definition.
function ParamSpec.compositor(name, default, opts)
    opts = opts or {}
    return {
        name = name,
        type = "compositor",
        default = default or "",
        label = opts.label or name
    }
end

--- Declare a complete ParamSpec from an array of parameter definitions.
--- Returns a table with get/set methods for accessing parameter values.
function ParamSpec.declare(paramDefs)
    local spec = {
        _defs = paramDefs,
        _values = {}
    }

    -- Initialize values from defaults
    for _, def in ipairs(paramDefs) do
        if def.type == "color" or def.type == "vec3" then
            -- Copy table values
            spec._values[def.name] = {table.unpack(def.default)}
        else
            spec._values[def.name] = def.default
        end
    end

    function spec:get(name)
        return self._values[name]
    end

    function spec:set(name, value)
        self._values[name] = value
    end

    function spec:getDefs()
        return self._defs
    end

    return spec
end

return ParamSpec
