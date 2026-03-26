-- logger.lua — BBFx v2.6 Structured Logger
-- Logger.info/warn/error → stdout + optional file

Logger = {}
Logger._file = nil
Logger._level_names = {INFO = "INFO", WARN = "WARN", ERROR = "ERROR"}

function Logger.init(filename)
    if Logger._file then
        Logger._file:close()
    end
    local f, err = io.open(filename, "a")
    if f then
        Logger._file = f
    else
        print("[Logger] Cannot open log file: " .. tostring(err))
    end
end

local function log_msg(level, msg)
    local ts = os.date("%Y-%m-%d %H:%M:%S")
    local line = "[" .. ts .. "] [" .. level .. "] " .. tostring(msg)
    print(line)
    if Logger._file then
        Logger._file:write(line .. "\n")
        Logger._file:flush()
    end
end

function Logger.info(msg)
    log_msg("INFO", msg)
end

function Logger.warn(msg)
    log_msg("WARN", msg)
end

function Logger.error(msg)
    log_msg("ERROR", msg)
end

function Logger.close()
    if Logger._file then
        Logger._file:close()
        Logger._file = nil
    end
end
