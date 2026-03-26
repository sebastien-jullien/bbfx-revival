-- threads.lua — BBFx v2.3 Coroutine Scheduler
-- Ported from 2006 production code (prog/workspace/bbfx/bbfx/src/lua/0.1/threads.lua)

Thread = {
    threads = {},
    scheduler = nil
}

-- Create a new thread (coroutine)
function Thread:new(func)
    local t = {}
    t.co = coroutine.create(func)
    t.running = false
    table.insert(Thread.threads, t)
    return t
end

-- Start a thread
function Thread:start()
    self.running = true
end

-- Sleep for a number of seconds (yield from coroutine)
function Thread.sleep(seconds)
    local elapsed = 0
    while elapsed < seconds do
        local dt = coroutine.yield()
        elapsed = elapsed + (dt or 0)
    end
end

-- Schedule all threads for one frame
-- Call this from a LuaAnimationNode update
function Thread.schedule(dt)
    local toRemove = {}
    for i, t in ipairs(Thread.threads) do
        if t.running and coroutine.status(t.co) ~= "dead" then
            local ok, err = coroutine.resume(t.co, dt)
            if not ok then
                print("[thread] Error: " .. tostring(err))
                t.running = false
            end
            if coroutine.status(t.co) == "dead" then
                t.running = false
                toRemove[#toRemove + 1] = i
            end
        end
    end
    -- Cleanup dead threads
    for i = #toRemove, 1, -1 do
        table.remove(Thread.threads, toRemove[i])
    end
end

-- Initialize the scheduler as a LuaAnimationNode
function Thread.init()
    if Thread.scheduler then return end
    Thread.scheduler = bbfx.LuaAnimationNode("ThreadScheduler", function(self)
        local dtPort = self:getInput("dt")
        if dtPort then
            Thread.schedule(dtPort:getValue())
        end
    end)
    Thread.scheduler:addInput("dt")

    local animator = bbfx.Animator.instance()
    local tn = bbfx.RootTimeNode.instance()
    animator:addNode(Thread.scheduler)
    animator:addPort(tn, "dt", Thread.scheduler, "dt")
end
