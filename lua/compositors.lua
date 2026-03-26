-- compositors.lua — BBFx v2.3 Compositor Manager Wrapper

Compositor = {
    active = {}
}

-- Get the main viewport
local function getViewport()
    return engine:getRenderWindow():getViewport(0)
end

-- Add a compositor to the viewport chain
function Compositor.add(name)
    local vp = getViewport()
    local mgr = Ogre.CompositorManager.getSingleton()
    mgr:addCompositor(vp, name)
    mgr:setCompositorEnabled(vp, name, true)
    Compositor.active[name] = true
    print("[compositor] Added: " .. name)
end

-- Remove a compositor from the chain
function Compositor.remove(name)
    local vp = getViewport()
    local mgr = Ogre.CompositorManager.getSingleton()
    mgr:setCompositorEnabled(vp, name, false)
    mgr:removeCompositor(vp, name)
    Compositor.active[name] = nil
    print("[compositor] Removed: " .. name)
end

-- Toggle a compositor enabled/disabled
function Compositor.toggle(name)
    local vp = getViewport()
    local mgr = Ogre.CompositorManager.getSingleton()
    if Compositor.active[name] then
        mgr:setCompositorEnabled(vp, name, false)
        Compositor.active[name] = nil
        print("[compositor] Disabled: " .. name)
    else
        -- Add if not already in chain
        mgr:addCompositor(vp, name)
        mgr:setCompositorEnabled(vp, name, true)
        Compositor.active[name] = true
        print("[compositor] Enabled: " .. name)
    end
end

-- List active compositors
function Compositor.list()
    local result = {}
    for name, _ in pairs(Compositor.active) do
        result[#result + 1] = name
    end
    return result
end
