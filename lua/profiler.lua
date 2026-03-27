-- profiler.lua — BBFx v2.8 Performance Profiling Overlay
-- Shows frame time, draw calls, triangles

require 'helpers'
require 'hud'

local _profVisible = false
local _profText = nil
local _profOverlay = nil

-- Bind RenderTarget::getStatistics if available
local function getStats()
    local ok, stats = pcall(function()
        local rw = bbfx.Engine.instance():getRenderWindow()
        return rw:getStatistics()
    end)
    if ok and stats then return stats end
    return nil
end

function perf()
    if not _profOverlay then
        -- Create profiler overlay elements
        local ok, err = pcall(function()
            _profOverlay = Ogre.OverlayManager_create(UID("prof/"))
            _profOverlay:setZOrder(600)

            local panel = Ogre.OverlayManager_createPanel(UID("prof_panel/"))
            panel:setPosition(0.0, 0.92)
            panel:setDimensions(1.0, 0.08)

            _profText = Ogre.OverlayManager_createTextArea(UID("prof_text/"))
            _profText:setPosition(0.01, 0.0)
            _profText:setDimensions(0.98, 0.06)
            _profText:setCharHeight(0.025)
            _profText:setCaption("Profiler loading...")
            _profText:setColour(Ogre.ColourValue(0, 1, 0, 1))

            panel:addChild(_profText)
            _profOverlay:add2D(panel)
        end)
        if not ok then
            print("[profiler] Cannot create overlay: " .. tostring(err))
            return
        end

        -- Create update node
        local profNode = bbfx.LuaAnimationNode(UID("profiler/"), function(self)
            if not _profVisible or not _profText then return end
            local dtPort = self:getInput("dt")
            if not dtPort then return end
            local dt = dtPort:getValue()
            local frameMs = dt * 1000.0

            local caption = string.format("Frame: %.1fms (%.0f fps)", frameMs, 1.0 / math.max(dt, 0.001))
            _profText:setCaption(caption)
        end)
        profNode:addInput("dt")
        local animator = bbfx.Animator.instance()
        local tn = bbfx.RootTimeNode.instance()
        animator:addNode(profNode)
        animator:addPort(tn, "dt", profNode, "dt")
    end

    _profVisible = not _profVisible
    if _profVisible then
        _profOverlay:show()
        print("[profiler] ON")
    else
        _profOverlay:hide()
        print("[profiler] OFF")
    end
end
