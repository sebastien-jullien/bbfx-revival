-- Single-swap focused test. Capture pre and IMMEDIATELY POST first swap.
return {
    name = "_diag_swap_repro",
    bpm  = 120,
    description = "single swap focused",
    setup = function()
        local tn = bbfx.RootTimeNode.instance()
        if tn then tn:setBPM(120) end
        dbg.create("FullscreenOverlayNode", "overlay")
        dbg.create_with_param("TheoraClipNode", "clip", "filename", "resources/video/bombe.ogg")
        _dbg_process_pending()
        dbg.set_param("overlay", "mode", "screen_aligned")
        dbg.set("overlay", "alpha", 1.0)
        dbg.set_param("clip", "reverse_filename", "resources/video/bombe_reverse.ogg")
        dbg.link("time", "dt", "clip", "dt")
        dbg.link("clip", "material_ready", "overlay", "material_source")

        local anim = bbfx.Animator.instance()
        local clipNode = anim:getNodeByName("clip")
        local reversePort = clipNode:getInput("reverse")
        local playPort = clipNode:getInput("play")
        if playPort then playPort:setValue(1.0) end
        if reversePort then reversePort:setValue(0.0) end

        os.execute("mkdir -p output/swap_repro 2>nul")

        -- Longer warmup (= ensure clip is playing forward and texture is updated).
        local PLAN = {
            {wait=4.0, name="warmup"},
            {wait=0,   name="PRE", action="shot_pre"},
            {wait=0.03, name="FLIP", action="set_rev_1"},
            {wait=0.05, name="POST1", action="shot_post1"},
            {wait=0.10, name="POST2", action="shot_post2"},
            {wait=0.20, name="POST3", action="shot_post3"},
            {wait=0.50, name="POST4", action="shot_post4"},
            {wait=0.5, name="done", action="done"},
        }
        local idx = 1
        local nextT = os.clock() + 0.3

        local driver = bbfx.LuaAnimationNode("_repro", function(self)
            local now = os.clock()
            if now < nextT then return end
            if idx > #PLAN then return end
            local step = PLAN[idx]
            local act = step.action
            if act == "shot_pre" then
                dbg.screenshot("output/swap_repro/SINGLE_PRE.png")
                print("[repro] PRE")
            elseif act == "set_rev_1" then
                if reversePort then reversePort:setValue(1.0) end
                print("[repro] reverse:=1")
            elseif act == "shot_post1" then
                dbg.screenshot("output/swap_repro/SINGLE_POST1_50ms.png")
                print("[repro] POST1 (50ms after swap)")
            elseif act == "shot_post2" then
                dbg.screenshot("output/swap_repro/SINGLE_POST2_150ms.png")
                print("[repro] POST2 (150ms)")
            elseif act == "shot_post3" then
                dbg.screenshot("output/swap_repro/SINGLE_POST3_350ms.png")
                print("[repro] POST3 (350ms)")
            elseif act == "shot_post4" then
                dbg.screenshot("output/swap_repro/SINGLE_POST4_850ms.png")
                print("[repro] POST4 (850ms)")
            elseif act == "done" then
                print("[repro] DONE")
                os.exit(0)
            end
            idx = idx + 1
            if idx <= #PLAN then nextT = now + PLAN[idx].wait end
        end)
        driver:addInput("dt")
        anim:addNode(driver)
        if tn then anim:addPort(tn, "dt", driver, "dt") end
    end
}
