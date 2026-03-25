-- benchmark.lua — BBFx Performance Benchmarks
-- Usage: bbfx.exe tests/benchmark.lua

local BENCHMARK_FRAMES = 300
print("=== BBFx Performance Benchmarks ===")

local startup_begin = os.clock()
local engine = bbfx.Engine.instance()
local scene = engine:getSceneManager()
scene:setAmbientLight(Ogre.ColourValue(0.3, 0.3, 0.3))
local bench_node = scene:getRootSceneNode():createChildSceneNode("BenchNode")
local startup_end = os.clock()
local startup_ms = (startup_end - startup_begin) * 1000.0
print(string.format("[bench] Startup time: %.1f ms", startup_ms))

local tn = bbfx.RootTimeNode.instance()
local animator = bbfx.Animator.instance()

local frame_count = 0
local total_frame_time = 0
local min_frame_time = math.huge
local max_frame_time = 0
local lua_calls = 0
local lua_total_us = 0

local bench = bbfx.LuaAnimationNode("bench", function(self)
    local t0 = os.clock()
    bench_node:yaw(Ogre.Radian(0.02))
    local t1 = os.clock()
    lua_calls = lua_calls + 1
    lua_total_us = lua_total_us + (t1 - t0) * 1e6

    local p = self:getInput("dt")
    if p then
        local dt = p:getValue()
        if type(dt) == "number" and dt > 0 then
            frame_count = frame_count + 1
            local ms = dt * 1000
            total_frame_time = total_frame_time + ms
            if ms < min_frame_time then min_frame_time = ms end
            if ms > max_frame_time then max_frame_time = ms end

            if frame_count >= BENCHMARK_FRAMES then
                local avg = total_frame_time / frame_count
                local fps = 1000 / avg
                print(string.format("\n[bench] Frames: %d", frame_count))
                print(string.format("[bench] Avg frame: %.2f ms (%.1f fps)", avg, fps))
                print(string.format("[bench] Min/Max: %.2f / %.2f ms", min_frame_time, max_frame_time))
                if lua_calls > 0 then
                    print(string.format("[bench] Lua overhead: %.2f us/call", lua_total_us / lua_calls))
                end
                print("")
                print(fps >= 30 and "[bench] PASS: >= 30 fps (NFR-P01)" or "[bench] WARN: < 30 fps (NFR-P01)")
                print((lua_calls > 0 and lua_total_us/lua_calls < 500) and "[bench] PASS: Lua < 0.5ms (NFR-P02)" or "[bench] WARN: Lua >= 0.5ms (NFR-P02)")
                print(startup_ms < 5000 and "[bench] PASS: Startup < 5s (NFR-P03)" or "[bench] WARN: Startup >= 5s (NFR-P03)")
                print("\n=== Benchmarks complete ===")
                engine:stopRendering()
            end
        end
    end
end)
bench:addInput("dt")
animator:addNode(tn)
animator:addNode(bench)
animator:addPort(tn, "dt", bench, "dt")
print("[bench] Running " .. BENCHMARK_FRAMES .. " frames...")
