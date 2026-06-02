-- poc_pause_reverse_builder.lua — Lot AV.5 round 33
--
-- Reproduit le bug exact rapporté par l'user :
--   1. Clip plays forward.
--   2. Pause.
--   3. Click reverse (en pause).
--   4. Un-pause.
--   5. → "ça ne bouge plus" (deadlock entre consumer/producer).
--
-- Test : après unpause, on attend N ticks et on vérifie que le clip joue
-- effectivement (= idx change entre 2 captures). Si l'idx ne change pas
-- pendant N ticks, le clip est stuck = FAIL.

return {
    name = "POC Pause Reverse Unpause Deadlock Test",
    bpm  = 120,
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
        _dbg_process_pending()

        local animator = bbfx.Animator.instance()
        local function log(m) print("[poc_pr] " .. m) end

        local st = { frame = 0, phase = "warmup", step_frame = 0, idx_before_pause = nil,
                     idx_after_unpause_1 = nil, idx_after_unpause_2 = nil,
                     cycle = 0, cycles_target = 3 }

        local ctrl = bbfx.LuaAnimationNode("_dbg_poc_pr_ctrl", function(self)
            st.frame = st.frame + 1
            local f = st.frame
            local p = st.phase

            if f == 1 then log("controller alive — running pause+reverse+unpause × 3") end

            if p == "warmup" then
                if f >= 60 then
                    st.cycle = 1
                    st.phase = "play_phase"
                    st.step_frame = f
                    log(string.format("=== CYCLE %d/%d ===", st.cycle, st.cycles_target))
                end

            elseif p == "play_phase" then
                -- Let it play forward for 30 ticks then pause.
                if f - st.step_frame >= 30 then
                    st.idx_before_pause = dbg.video_blitted_index("clip")
                    log(string.format("[cycle %d] PRE-pause idx=%d", st.cycle, st.idx_before_pause))
                    dbg.set("clip", "play", 0)   -- pause
                    st.phase = "paused_pre_rev"
                    st.step_frame = f
                end

            elseif p == "paused_pre_rev" then
                -- Wait a few ticks in pause then click reverse.
                if f - st.step_frame >= 10 then
                    local idx_paused = dbg.video_blitted_index("clip")
                    log(string.format("[cycle %d] in pause, idx=%d (should be ~%d)",
                        st.cycle, idx_paused, st.idx_before_pause))
                    -- Toggle reverse via the port (= same as gamepad would do).
                    -- Use video_set_reverse direct API (= same effect, simpler).
                    local was_rev = dbg.video_is_reversed("clip")
                    dbg.video_set_reverse("clip", not was_rev)
                    log(string.format("[cycle %d] setReverse(%s) called in pause",
                        st.cycle, tostring(not was_rev)))
                    st.phase = "paused_post_rev"
                    st.step_frame = f
                end

            elseif p == "paused_post_rev" then
                -- Wait a few ticks in pause then un-pause.
                if f - st.step_frame >= 10 then
                    local idx_now = dbg.video_blitted_index("clip")
                    log(string.format("[cycle %d] still paused post-setReverse, idx=%d",
                        st.cycle, idx_now))
                    dbg.set("clip", "play", 1)   -- unpause
                    log(string.format("[cycle %d] UNPAUSE", st.cycle))
                    st.phase = "post_unpause_wait"
                    st.step_frame = f
                end

            elseif p == "post_unpause_wait" then
                -- Wait 10 ticks then capture idx.
                if f - st.step_frame >= 10 then
                    st.idx_after_unpause_1 = dbg.video_blitted_index("clip")
                    log(string.format("[cycle %d] T+10 after unpause: idx=%d",
                        st.cycle, st.idx_after_unpause_1))
                    st.phase = "post_unpause_verify"
                    st.step_frame = f
                end

            elseif p == "post_unpause_verify" then
                -- Wait 30 more ticks (= clip should have played a bit). If idx
                -- hasn't moved, we're stuck.
                if f - st.step_frame >= 30 then
                    st.idx_after_unpause_2 = dbg.video_blitted_index("clip")
                    local delta = st.idx_after_unpause_2 - st.idx_after_unpause_1
                    log(string.format("[cycle %d] T+40 after unpause: idx=%d delta=%+d",
                        st.cycle, st.idx_after_unpause_2, delta))
                    local stuck = (delta == 0)
                    if stuck then
                        log(string.format("[cycle %d] FAIL — STUCK (idx unchanged for 30 ticks after unpause)",
                            st.cycle))
                    else
                        log(string.format("[cycle %d] OK — clip is playing (delta=%+d frames)",
                            st.cycle, delta))
                    end

                    if st.cycle >= st.cycles_target then
                        st.phase = "done"
                        st.step_frame = f
                        st.last_was_stuck = stuck
                    else
                        st.cycle = st.cycle + 1
                        st.phase = "play_phase"
                        st.step_frame = f
                        st.last_was_stuck = stuck
                        log(string.format("=== CYCLE %d/%d ===", st.cycle, st.cycles_target))
                    end
                end

            elseif p == "done" then
                if f - st.step_frame >= 10 then
                    log("")
                    log("=========================================================")
                    log("=== POC PAUSE REVERSE UNPAUSE — DONE ===")
                    log("=========================================================")
                    if st.last_was_stuck then
                        log(">>> RESULT: FAIL — pause+reverse+unpause STUCK")
                        os.exit(1)
                    else
                        log(">>> RESULT: PASS — pause+reverse+unpause works")
                        os.exit(0)
                    end
                end
            end
        end)
        ctrl:addInput("dt")
        animator:addNode(ctrl)
        if tn then animator:addPort(tn, "dt", ctrl, "dt") end
        log("setup complete")
    end
}
