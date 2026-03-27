-- exporter.lua — BBFx v2.9 Video Exporter
-- Captures each rendered frame as a PNG file

require 'helpers'
require 'logger'

local _exporter = nil

function isexporting()
    return _exporter and _exporter:isExporting() or false
end

function export_start(outputDir)
    if isexporting() then
        Logger.warn("[exporter] Already exporting. Use stopexport() first.")
        return
    end
    _exporter = bbfx.VideoExporter()
    _exporter:start(outputDir or "output/frames")
    bbfx.Engine.instance():setVideoExporter(_exporter)

    Logger.info("[exporter] Exporting to: " .. (outputDir or "output/frames"))
end

function stopexport()
    if isexporting() then
        bbfx.Engine.instance():clearVideoExporter(_exporter)
        _exporter:stop()
        Logger.info("[exporter] Export stopped: " .. _exporter:getFrameCount() .. " frames")
    else
        Logger.warn("[exporter] Not exporting")
    end
end

function export_toggle(outputDir)
    if isexporting() then
        stopexport()
        return false
    end

    export_start(outputDir)
    return true
end
