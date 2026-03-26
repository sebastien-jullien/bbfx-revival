-- demo_fanions.lua — BBFx v2.5
-- Port of 2006 production setFanions.textures.lua
-- Demonstrates TextureCycle + TextureSet with preset {gray, color, factor?, rotate?} table
-- Textures may not be present in resources — missing textures are handled gracefully (no crash)

require 'helpers'
require 'textureset'

bbfx_globals()

print("[demo_fanions] BBFx v2.5 — Fanions Texture Set Demo")
print("  ESC: Quit")
print("  N: Next texture preset in cycle 1")
print("  M: Next texture preset in cycle 2")
print("  S: Swap active TextureSet")
print("")

-- Helper: build preset list from raw table {[1]=gray, [2]=color, rotate=bool, factor=num}
local function build_presets(t)
    local r = {}
    for _, v in ipairs(t) do
        table.insert(r, {
            gray   = v[1],
            color  = v[2],
            rotate = v.rotate,
            factor = v.factor or -0.7
        })
    end
    return r
end

-- Raw preset definitions (port of 2006 setFanions.textures.lua)
local raw_presets = {
    {'fern2.jpg',              'ferncolor.png',              rotate=true,  factor=-0.1},
    {'water.png',              'mywater.jpg',                              factor=-0.6},
    {'indigo_lasers.jpg',      'streaming_protons.png',                    factor=-3.0},
    {'folded_space.png',       'petals.jpg'},
    {'08b.jpg',                'frac385-2.jpg',              rotate=true,  factor=-0.5},
    {'electric_nebula.png',    'subliminal_spectrum.jpg'},
    {'silver_vibration.png',   'flaming_aurora.jpg'},
    {'penrosef.jpg',           'electric_nebula.jpg',        rotate=true,  factor=-3.0},
    {'frac321.jpg',            'plasma.png',                 rotate=true,  factor=-5.0},
    {'lace.jpg',               '204-CMYK Fire.jpg',          rotate=true,  factor=-1.0},
}

-- Split presets into two interleaved cycles (port of mux() from 2006)
local function mux(t)
    local r1, r2 = {}, {}
    for i, v in ipairs(t) do
        if i % 2 == 1 then
            table.insert(r1, v)
        else
            table.insert(r2, v)
        end
    end
    return r1, r2
end

local presets = build_presets(raw_presets)
local presets1, presets2 = mux(presets)

-- Create TextureCycles
local cycle1 = TextureCycle:new(presets1)
local cycle2 = TextureCycle:new(presets2)

-- Settings (port of 2006 setFanions.textures — joystick optional)
local settings = {
    uscroll     = 1/60,
    vscroll     = 1/90,
    scrollbutton = 1,
    rspeed      = 1/150,
    sweep       = 'gradient.png',
    vsweep      = 1/5
}

local function clone_settings(base)
    local t = {}
    for k, v in pairs(base) do t[k] = v end
    return t
end

local settings1 = clone_settings(settings)
settings1.sweepbutton = 0
settings1.sweepstate  = 1

local settings2 = clone_settings(settings)
settings2.sweepstate  = 2

-- Create TextureSets
local texSet  = TextureSet:new(settings1, cycle1)
local texSet2 = TextureSet:new(settings2, cycle2)
texSet:on()
texSet:setSwap(texSet2)

print("[demo_fanions] TextureSet 1 active: " .. tostring(texSet.active))
print("[demo_fanions] TextureSet 2 active: " .. tostring(texSet2.active))
print("[demo_fanions] Swap linked: " .. tostring(texSet._swap ~= nil))
print("[demo_fanions] Cycle 1 size: " .. tostring(#cycle1.textures) .. " presets")
print("[demo_fanions] Cycle 2 size: " .. tostring(#cycle2.textures) .. " presets")

-- Print current preset info
local function print_preset(label, cycle)
    local p = cycle:current()
    if p then
        print(string.format("  %s: gray=%s  color=%s  factor=%.2f  rotate=%s",
            label, tostring(p.gray), tostring(p.color),
            p.factor or -0.7, tostring(p.rotate or false)))
    end
end
print_preset("Cycle1 current", cycle1)
print_preset("Cycle2 current", cycle2)

-- Swap helper
local activeSet = texSet
local function do_swap()
    if activeSet._swap then
        activeSet:off()
        activeSet = activeSet._swap
        activeSet:on()
        print("[demo_fanions] Swapped — active: " .. tostring(activeSet.settings.sweepstate))
    end
end

-- Animation node for keyboard interaction
local animator = bbfx.Animator.instance()
local tn       = bbfx.RootTimeNode.instance()

local updateNode = bbfx.LuaAnimationNode(UID("fanions/"), function(self_node)
    local dtPort = self_node:getInput("dt")
    if not dtPort then return end

    if keyboard:wasKeyPressed(110) then -- 'n'
        cycle1:next()
        print_preset("Cycle1 next", cycle1)
    end
    if keyboard:wasKeyPressed(109) then -- 'm'
        cycle2:next()
        print_preset("Cycle2 next", cycle2)
    end
    if keyboard:wasKeyPressed(115) then -- 's'
        do_swap()
    end
end)
updateNode:addInput("dt")
animator:addNode(tn)
animator:addNode(updateNode)
animator:addPort(tn, "dt", updateNode, "dt")

print("[demo_fanions] Ready — press ESC to exit.")
