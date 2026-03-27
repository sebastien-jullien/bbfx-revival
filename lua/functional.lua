-- functional.lua — BBFx v2.9 Functional Lua Utilities
-- map, filter, reduce, keys, values

function map(t, fn)
    local r = {}
    for i, v in ipairs(t) do
        r[i] = fn(v, i)
    end
    return r
end

function filter(t, fn)
    local r = {}
    for _, v in ipairs(t) do
        if fn(v) then
            r[#r + 1] = v
        end
    end
    return r
end

function reduce(t, fn, init)
    local acc = init
    for _, v in ipairs(t) do
        acc = fn(acc, v)
    end
    return acc
end

function keys(t)
    local r = {}
    for k, _ in pairs(t) do
        r[#r + 1] = k
    end
    return r
end

function values(t)
    local r = {}
    for _, v in pairs(t) do
        r[#r + 1] = v
    end
    return r
end
