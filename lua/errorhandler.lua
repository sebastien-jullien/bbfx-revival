-- errorhandler.lua — BBFx v2.6 Error Recovery
-- ErrorHandler.eval/dofile with pcall + stack trace + Logger integration

require 'logger'

ErrorHandler = {}

-- Evaluate a Lua expression string safely
-- Returns: ok (bool), result_or_error (any)
function ErrorHandler.eval(expr)
    local fn, compile_err = load("return " .. expr)
    if not fn then
        -- Try as statement (no return)
        fn, compile_err = load(expr)
    end
    if not fn then
        local msg = "compile error: " .. tostring(compile_err)
        Logger.error(msg)
        return false, msg
    end
    local ok, result = xpcall(fn, debug.traceback)
    if not ok then
        Logger.error(tostring(result))
        return false, result
    end
    return true, result
end

-- Execute a Lua file safely
-- Returns: ok (bool), error_or_nil
function ErrorHandler.dofile(filename)
    local fn, compile_err = loadfile(filename)
    if not fn then
        local msg = "load error: " .. tostring(compile_err)
        Logger.error(msg)
        return false, msg
    end
    local ok, err = xpcall(fn, debug.traceback)
    if not ok then
        Logger.error(tostring(err))
        return false, err
    end
    return true, nil
end
