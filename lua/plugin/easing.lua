-- ============================================================================
-- BBFx v3.5 Lot N — pure-Lua easing library + lerp/bezier helpers
-- ============================================================================
-- Covers the standard 33 easing functions (Robert Penner + Bounce) plus
-- convenience helpers: lerp(a, b, t, name), lerpColor, lerpVec3, bezier.
--
-- All easing functions take t ∈ [0, 1] and return a value in ~[0, 1]
-- (back/elastic can overshoot by design).
-- ============================================================================

local M = {}

-- ── linear ─────────────────────────────────────────────────────────────────
function M.linear(t) return t end

-- ── quad ───────────────────────────────────────────────────────────────────
function M.easeInQuad(t)  return t * t end
function M.easeOutQuad(t) return t * (2 - t) end
function M.easeInOutQuad(t)
    if t < 0.5 then return 2 * t * t
    else            return -1 + (4 - 2 * t) * t end
end

-- ── cubic ──────────────────────────────────────────────────────────────────
function M.easeInCubic(t)  return t * t * t end
function M.easeOutCubic(t) local s = t - 1; return s * s * s + 1 end
function M.easeInOutCubic(t)
    if t < 0.5 then return 4 * t * t * t
    else
        local s = 2 * t - 2
        return 0.5 * s * s * s + 1
    end
end

-- ── sine ───────────────────────────────────────────────────────────────────
function M.easeInSine(t)   return 1 - math.cos(t * math.pi / 2) end
function M.easeOutSine(t)  return math.sin(t * math.pi / 2) end
function M.easeInOutSine(t) return -(math.cos(math.pi * t) - 1) / 2 end

-- ── expo ───────────────────────────────────────────────────────────────────
function M.easeInExpo(t)
    return (t == 0) and 0 or 2 ^ (10 * t - 10)
end
function M.easeOutExpo(t)
    return (t == 1) and 1 or 1 - 2 ^ (-10 * t)
end
function M.easeInOutExpo(t)
    if     t == 0 then return 0
    elseif t == 1 then return 1
    elseif t < 0.5 then return 2 ^ (20 * t - 10) / 2
    else return (2 - 2 ^ (-20 * t + 10)) / 2
    end
end

-- ── circ ───────────────────────────────────────────────────────────────────
function M.easeInCirc(t)   return 1 - math.sqrt(1 - t * t) end
function M.easeOutCirc(t)  local s = t - 1; return math.sqrt(1 - s * s) end
function M.easeInOutCirc(t)
    if t < 0.5 then return (1 - math.sqrt(1 - 4 * t * t)) / 2
    else
        local s = -2 * t + 2
        return (math.sqrt(1 - s * s) + 1) / 2
    end
end

-- ── back ───────────────────────────────────────────────────────────────────
local C1 = 1.70158
local C2 = C1 * 1.525
local C3 = C1 + 1
function M.easeInBack(t)  return C3 * t * t * t - C1 * t * t end
function M.easeOutBack(t)
    local s = t - 1
    return 1 + C3 * s * s * s + C1 * s * s
end
function M.easeInOutBack(t)
    if t < 0.5 then
        return (((2*t)^2) * ((C2 + 1) * 2*t - C2)) / 2
    else
        local s = 2*t - 2
        return ((s*s) * ((C2 + 1) * s + C2) + 2) / 2
    end
end

-- ── elastic ────────────────────────────────────────────────────────────────
local C4 = (2 * math.pi) / 3
local C5 = (2 * math.pi) / 4.5
function M.easeInElastic(t)
    if     t == 0 then return 0
    elseif t == 1 then return 1
    else return -2 ^ (10 * t - 10) * math.sin((t * 10 - 10.75) * C4)
    end
end
function M.easeOutElastic(t)
    if     t == 0 then return 0
    elseif t == 1 then return 1
    else return 2 ^ (-10 * t) * math.sin((t * 10 - 0.75) * C4) + 1
    end
end
function M.easeInOutElastic(t)
    if     t == 0 then return 0
    elseif t == 1 then return 1
    elseif t < 0.5 then
        return -(2 ^ (20 * t - 10) * math.sin((20 * t - 11.125) * C5)) / 2
    else
        return  (2 ^ (-20 * t + 10) * math.sin((20 * t - 11.125) * C5)) / 2 + 1
    end
end

-- ── bounce ─────────────────────────────────────────────────────────────────
local BN = 7.5625
local BD = 2.75
function M.easeOutBounce(t)
    if     t < 1/BD   then return BN * t * t
    elseif t < 2/BD   then local s = t - 1.5/BD;   return BN * s * s + 0.75
    elseif t < 2.5/BD then local s = t - 2.25/BD;  return BN * s * s + 0.9375
    else                  local s = t - 2.625/BD; return BN * s * s + 0.984375
    end
end
function M.easeInBounce(t)  return 1 - M.easeOutBounce(1 - t) end
function M.easeInOutBounce(t)
    if t < 0.5 then return (1 - M.easeOutBounce(1 - 2 * t)) / 2
    else            return (1 + M.easeOutBounce(2 * t - 1)) / 2
    end
end

-- ── helpers ────────────────────────────────────────────────────────────────
local function apply(t, name)
    local fn = M[name or "linear"]
    if not fn then return t end
    return fn(t)
end

function M.lerp(a, b, t, name)
    local e = apply(t, name)
    return a + (b - a) * e
end

function M.lerpColor(r1, g1, b1, r2, g2, b2, t, name)
    local e = apply(t, name)
    return r1 + (r2 - r1) * e,
           g1 + (g2 - g1) * e,
           b1 + (b2 - b1) * e
end

function M.lerpVec3(x1, y1, z1, x2, y2, z2, t, name)
    local e = apply(t, name)
    return x1 + (x2 - x1) * e,
           y1 + (y2 - y1) * e,
           z1 + (z2 - z1) * e
end

-- Cubic Bezier on a single parameter space (not a 2D curve).
-- p0..p3 are scalar control values; t is the parameter.
function M.bezier(t, p0, p1, p2, p3)
    local u  = 1 - t
    local tt = t * t
    local uu = u * u
    return uu * u * p0
         + 3 * uu * t * p1
         + 3 * u  * tt * p2
         + tt * t * p3
end

-- Convenience: list of all supported easing names (useful for UI combos).
function M.names()
    return {
        "linear",
        "easeInQuad", "easeOutQuad", "easeInOutQuad",
        "easeInCubic", "easeOutCubic", "easeInOutCubic",
        "easeInSine", "easeOutSine", "easeInOutSine",
        "easeInExpo", "easeOutExpo", "easeInOutExpo",
        "easeInCirc", "easeOutCirc", "easeInOutCirc",
        "easeInBack", "easeOutBack", "easeInOutBack",
        "easeInElastic", "easeOutElastic", "easeInOutElastic",
        "easeInBounce", "easeOutBounce", "easeInOutBounce",
    }
end

return M
