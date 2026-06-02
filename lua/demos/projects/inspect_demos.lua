-- inspect_demos.lua — BBFx v3.5.2 Sprint S8 Lot AU
--
-- Runs each demo builder, lets the scene render a few frames, captures a
-- screenshot to output/demo_shots/<name>.png, then moves to the next. Quits
-- via os.exit(0) when done. Visual validation of the Demo Showcase Pack.
--
-- (We run the builders directly rather than loading the baked .bbfx-project
--  files — dbg.load() is the deferred scripting shortcut and doesn't trigger
--  the full studio entity-restore path; File→Open Demo in the UI does.)
--
-- Usage (from build/windows-debug/Debug/):
--   ./bbfx-studio.exe lua/demos/projects/inspect_demos.lua

local DEMOS = {
    "demo_studio_base", "demo_mesh_morph", "demo_particle_garden",
    "demo_texture_set", "demo_video_wall", "demo_audio_reactive",
    "demo_shader_lab", "demo_vj_full", "demo_projection_mapping",
}

-- Per-demo timeline (frames): 0 = dbg.clear()  ; 3 = run builder
--                             50 = screenshot   ; 56 = (settle, next)
local FRAMES_PER = 56
local START      = 8
local frame = 0

local function tick()
    frame = frame + 1
    if frame < START then return end
    local rel = frame - START
    local di  = math.floor(rel / FRAMES_PER) + 1
    local sub = rel % FRAMES_PER

    if di > #DEMOS then
        print("[inspect_demos] done — screenshots in output/demo_shots/")
        os.exit(0); return
    end

    local name = DEMOS[di]
    if sub == 0 then
        dbg.clear()
    elseif sub == 3 then
        local ok, b = pcall(dofile, "lua/demos/projects/" .. name .. "_builder.lua")
        if ok and type(b) == "table" and type(b.setup) == "function" then
            local s, e = pcall(b.setup)
            if not s then print("[inspect_demos] " .. name .. " setup error: " .. tostring(e)) end
        else
            print("[inspect_demos] " .. name .. " builder load failed")
        end
        if _dbg_process_pending then _dbg_process_pending() end
        print("[inspect_demos] built " .. name)
    elseif sub == 50 then
        dbg.screenshot("output/demo_shots/" .. name .. ".png")
        print("[inspect_demos] shot: " .. name)
    end
end

_insp_anim = bbfx.Animator.instance()
_insp_node = bbfx.LuaAnimationNode("_dbg_inspect_demos", function(self) tick() end)
_insp_node:addInput("dt")
_insp_anim:addNode(_insp_node)
_insp_rt = bbfx.RootTimeNode.instance()
if _insp_rt then _insp_anim:addPort(_insp_rt, "dt", _insp_node, "dt") end
print("[inspect_demos] driver registered — inspecting " .. #DEMOS .. " demos")
