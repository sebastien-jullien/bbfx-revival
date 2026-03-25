-- benchmark.lua
-- Performance benchmarks for BBFx engine.
-- Usage: bbfx.exe tests/benchmark.lua
--
-- Measures:
--   1. Startup time (time from script load to first frame)
--   2. Frame time over 300 frames
--   3. Lua node overhead per frame

local BENCHMARK_FRAMES = 300

print("=== BBFx Performance Benchmarks ===")
print("")

-- ── Record startup time ────────────────────────────────────────────────────
local startup_begin = os.clock()

local engine = bbfx.Engine.instance()
local scene  = engine:getSceneManager()

scene:setAmbientLight(Ogre.ColourValue(0.3, 0.3, 0.3))

local light = scene:createLight("BenchLight")
light:setType(Ogre.Light.LT_POINT)
light:setDiffuseColour(Ogre.ColourValue(1, 1, 1))

local root_node = scene:getRootSceneNode()
local bench_node = root_node:createChildSceneNode("BenchNode")

local startup_end = os.clock()
local startup_ms = (startup_end - startup_begin) * 1000.0

print(string.format("[bench] Startup time (scene setup): %.1f ms", startup_ms))

-- ── Animator setup ─────────────────────────────────────────────────────────
local animator = bbfx.Animator.instance()
local time_node = bbfx.RootTimeNode("bench_timer")
animator:addNode(time_node)

local frame_count = 0
local total_frame_time = 0.0
local min_frame_time = math.huge
local max_frame_time = 0.0
local lua_node_calls = 0
local lua_node_total_us = 0.0

local bench_lua_node = bbfx.LuaAnimationNode("bench_lua", function(self)
    local t0 = os.clock()

    -- Simulate typical Lua node work: read input, compute, write output
    bench_node:yaw(Ogre.Radian(0.02))
    local pos = bench_node:getPosition()
    local _ = pos.x + pos.y + pos.z

    local t1 = os.clock()
    lua_node_calls = lua_node_calls + 1
    lua_node_total_us = lua_node_total_us + (t1 - t0) * 1e6
end)
animator:addNode(bench_lua_node)

-- Frame time measurement node
local frame_measure = bbfx.LuaAnimationNode("frame_measure", function(self)
    local dt_val = time_node:getOutput("dt"):getValue()
    if type(dt_val) == "number" and dt_val > 0 then
        frame_count = frame_count + 1
        local dt_ms = dt_val * 1000.0
        total_frame_time = total_frame_time + dt_ms
        if dt_ms < min_frame_time then min_frame_time = dt_ms end
        if dt_ms > max_frame_time then max_frame_time = dt_ms end

        if frame_count >= BENCHMARK_FRAMES then
            -- Print results and stop
            print("")
            print(string.format("[bench] Frames measured: %d", frame_count))
            print(string.format("[bench] Avg frame time: %.2f ms (%.1f fps)",
                total_frame_time / frame_count,
                1000.0 / (total_frame_time / frame_count)))
            print(string.format("[bench] Min frame time: %.2f ms", min_frame_time))
            print(string.format("[bench] Max frame time: %.2f ms", max_frame_time))

            if lua_node_calls > 0 then
                print(string.format("[bench] Lua node avg overhead: %.2f us/call (%d calls)",
                    lua_node_total_us / lua_node_calls, lua_node_calls))
            end

            -- Thresholds
            local avg_fps = 1000.0 / (total_frame_time / frame_count)
            print("")
            if avg_fps >= 30 then
                print("[bench] PASS: >= 30 fps (NFR-P01)")
            else
                print("[bench] WARN: < 30 fps (NFR-P01)")
            end

            if lua_node_calls > 0 and (lua_node_total_us / lua_node_calls) < 500 then
                print("[bench] PASS: Lua node overhead < 0.5 ms (NFR-P02)")
            else
                print("[bench] WARN: Lua node overhead >= 0.5 ms (NFR-P02)")
            end

            if startup_ms < 5000 then
                print("[bench] PASS: Startup < 5 seconds (NFR-P03)")
            else
                print("[bench] WARN: Startup >= 5 seconds (NFR-P03)")
            end

            print("")
            print("=== Benchmarks complete ===")
            engine:stopRendering()
        end
    end
end)
animator:addNode(frame_measure)

animator:addPort(time_node, "dt", frame_measure, "dt")

print("[bench] Running benchmark for " .. BENCHMARK_FRAMES .. " frames...")
