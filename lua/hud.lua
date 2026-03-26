-- hud.lua — BBFx v2.7 Audio HUD Overlay
-- Displays BPM + low/mid/high band levels in real-time

require 'helpers'

HUD = {}
HUD.__index = HUD

local _instance = nil

function HUD:new()
    if _instance then return _instance end

    local o = {}
    setmetatable(o, HUD)
    o._visible = false
    o._overlay = nil
    o._textBPM = nil
    o._textBands = nil

    -- Create OGRE overlay
    local ok, err = pcall(function()
        o._overlay = Ogre.OverlayManager_create(UID("hud/"))
        o._overlay:setZOrder(500)

        local panel = Ogre.OverlayManager_createPanel(UID("hud_panel/"))
        panel:setPosition(0.7, 0.02)
        panel:setDimensions(0.28, 0.12)

        o._textBPM = Ogre.OverlayManager_createTextArea(UID("hud_bpm/"))
        o._textBPM:setPosition(0.0, 0.0)
        o._textBPM:setDimensions(0.28, 0.04)
        o._textBPM:setCharHeight(0.03)
        o._textBPM:setCaption("BPM: --")
        o._textBPM:setColour(Ogre.ColourValue(1, 1, 0, 1))

        o._textBands = Ogre.OverlayManager_createTextArea(UID("hud_bands/"))
        o._textBands:setPosition(0.0, 0.04)
        o._textBands:setDimensions(0.28, 0.08)
        o._textBands:setCharHeight(0.025)
        o._textBands:setCaption("Low: --  Mid: --  High: --")
        o._textBands:setColour(Ogre.ColourValue(0.8, 0.8, 1, 1))

        panel:addChild(o._textBPM)
        panel:addChild(o._textBands)
        o._overlay:add2D(panel)
    end)

    if not ok then
        print("[HUD] Warning: could not create overlay — " .. tostring(err))
    end

    _instance = o
    return o
end

function HUD:show()
    if self._overlay then
        self._overlay:show()
        self._visible = true
    end
end

function HUD:hide()
    if self._overlay then
        self._overlay:hide()
        self._visible = false
    end
end

function HUD:toggle()
    if self._visible then self:hide() else self:show() end
end

function HUD:update(audioInstance)
    if not self._visible or not self._textBPM then return end
    if not audioInstance then
        self._textBPM:setCaption("BPM: --")
        self._textBands:setCaption("Low: --  Mid: --  High: --")
        return
    end

    local bpm = audioInstance:getBPM()
    local low = audioInstance:getBand("low")
    local mid = audioInstance:getBand("mid")
    local high = audioInstance:getBand("high")
    local rms = audioInstance:getRMS()

    self._textBPM:setCaption(string.format("BPM: %.0f  RMS: %.2f", bpm, rms))
    self._textBands:setCaption(string.format("Low: %.2f  Mid: %.2f  High: %.2f", low, mid, high))
end

-- Global toggle command
function hud()
    if _instance then
        _instance:toggle()
        print("[HUD] " .. (_instance._visible and "ON" or "OFF"))
    else
        print("[HUD] Not initialized. Create with HUD:new() first.")
    end
end
