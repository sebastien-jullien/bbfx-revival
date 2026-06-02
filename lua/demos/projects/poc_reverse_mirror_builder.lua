-- poc_reverse_mirror_builder.lua — BBFx v3.5.2 Sprint S8 Lot AV.5 round 29 (I-2056)
--
-- POC isolé pour mesurer OBJECTIVEMENT la dérive du mirror reverse↔forward.
-- Run : `./bbfx-studio.exe --demo poc_reverse_mirror` depuis Debug/.
-- Auto-exit 0 si tous les drifts <= TOLERANCE, sinon 1.
--
-- Modes :
--   MODE = "static" : pause clip, swap, vérifie idx identique sur N swaps.
--                     Mesure la précision du seek mirror seul (pas de consumer-lag).
--   MODE = "live"   : speed=1 continu, aller-retours symétriques de durée D.
--                     Mesure le bug "play actif" : après un round-trip complet
--                     (fwd→rev de durée D, puis rev→fwd de durée D), l'idx forward
--                     visible doit revenir à la baseline (à quelques frames de
--                     consumer-lag près). Si ce n'est pas le cas, le mirror dérive
--                     en présence d'activité décodage continue.

return {
    name = "POC Reverse Mirror Drift Test",
    bpm  = 120,
    description = "Mesure objective de la dérive du mirror reverse — modes static + live.",
    setup = function()
        ----------------------------------------------------------------------
        -- Config
        ----------------------------------------------------------------------
        local MODE             = os.getenv("POC_MODE") or "live"  -- "static" | "live"
        local ROUND_TRIPS      = tonumber(os.getenv("POC_ROUNDS"))    or 6
        local SETTLE_FRAMES    = tonumber(os.getenv("POC_SETTLE"))    or 45
        local LIVE_HALF_PERIOD = tonumber(os.getenv("POC_LIVE_HP"))   or 30
        local WARMUP_FRAMES    = tonumber(os.getenv("POC_WARMUP"))    or 60
        local TOLERANCE_FRAMES = tonumber(os.getenv("POC_TOL"))       or 2

        ----------------------------------------------------------------------
        -- Nodes
        ----------------------------------------------------------------------
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

        ----------------------------------------------------------------------
        -- Helpers
        ----------------------------------------------------------------------
        local animator = bbfx.Animator.instance()
        local function log(m) print("[poc] " .. m) end
        local function snapshot(name)
            dbg.screenshot("output/poc_reverse/" .. MODE .. "/" .. name .. ".png")
        end
        local function pause_clip()  dbg.set("clip", "speed", 0.0) end
        local function play_clip()   dbg.set("clip", "speed", 1.0) end
        local function set_rev(rev)  dbg.video_set_reverse("clip", rev) end
        local function idx()         return dbg.video_blitted_index("clip") end
        local function btime()       return dbg.video_blitted_time("clip") end

        ----------------------------------------------------------------------
        -- State
        ----------------------------------------------------------------------
        local st = {
            frame        = 0,
            phase        = "warmup",
            step_frame   = 0,
            round        = 0,
            idx_baseline = nil,
            results      = {},   -- liste { round, dir, idx, drift, t }
        }

        ----------------------------------------------------------------------
        -- Logic per-mode (state machine)
        ----------------------------------------------------------------------
        local function tick_static(f)
            local p = st.phase
            if p == "warmup" then
                if f >= WARMUP_FRAMES then
                    pause_clip()
                    st.phase = "baseline_wait"
                    st.step_frame = f
                end
            elseif p == "baseline_wait" then
                if f - st.step_frame >= 15 then
                    st.idx_baseline = idx()
                    log(string.format("BASELINE [static]: idx=%d / %d  t=%.3fs",
                        st.idx_baseline, dbg.video_total_frames("clip"), btime()))
                    snapshot(string.format("00_baseline_idx%d", st.idx_baseline))
                    st.round = 1
                    st.phase = "to_rev"
                    st.step_frame = f
                    log(string.format("=== ROUND %d/%d → reverse [static] ===", st.round, ROUND_TRIPS))
                    set_rev(true)
                end
            elseif p == "to_rev" then
                if f - st.step_frame >= SETTLE_FRAMES then
                    local i = idx()
                    local d = i - st.idx_baseline
                    log(string.format("  POST reverse r%d: idx=%d t=%.3fs drift=%+d",
                        st.round, i, btime(), d))
                    snapshot(string.format("r%02d_a_rev_idx%d", st.round, i))
                    table.insert(st.results, { round=st.round, dir="rev", idx=i, drift=d, t=btime() })
                    st.phase = "to_fwd"
                    st.step_frame = f
                    set_rev(false)
                end
            elseif p == "to_fwd" then
                if f - st.step_frame >= SETTLE_FRAMES then
                    local i = idx()
                    local d = i - st.idx_baseline
                    log(string.format("  POST forward r%d: idx=%d t=%.3fs drift=%+d",
                        st.round, i, btime(), d))
                    snapshot(string.format("r%02d_b_fwd_idx%d", st.round, i))
                    table.insert(st.results, { round=st.round, dir="fwd", idx=i, drift=d, t=btime() })
                    if st.round >= ROUND_TRIPS then
                        st.phase = "done"
                        st.step_frame = f
                    else
                        st.round = st.round + 1
                        st.phase = "to_rev"
                        st.step_frame = f
                        log(string.format("=== ROUND %d/%d → reverse [static] ===", st.round, ROUND_TRIPS))
                        set_rev(true)
                    end
                end
            end
        end

        ----------------------------------------------------------------------
        -- Mode LIVE :
        -- Le clip joue à speed=1 en continu (pas de pause). À chaque
        -- demi-période (LIVE_HALF_PERIOD frames), on flip reverse.
        -- Après un aller (fwd→rev sur durée D) + retour (rev→fwd sur durée D),
        -- on est censé revenir à idx_baseline.
        --
        -- Exemple : baseline idx=I0 (forward, frame 60).
        --   t=60 : swap to reverse. Reverse joue, idx forward visible décroît.
        --   t=90 : aurait dû atteindre idx = I0 - 30 (en coord fwd) ; swap to forward.
        --          Forward joue, idx forward visible croît.
        --   t=120 : idx visible doit revenir à I0.
        ----------------------------------------------------------------------
        local function tick_live(f)
            local p = st.phase
            if p == "warmup" then
                if f >= WARMUP_FRAMES then
                    st.idx_baseline = idx()
                    log(string.format("BASELINE [live]: idx=%d / %d  t=%.3fs",
                        st.idx_baseline, dbg.video_total_frames("clip"), btime()))
                    snapshot(string.format("00_baseline_idx%d", st.idx_baseline))
                    st.round = 1
                    st.phase = "live_to_rev"
                    st.step_frame = f
                    log(string.format("=== ROUND %d/%d swap → reverse (idx pre=%d) ===",
                        st.round, ROUND_TRIPS, st.idx_baseline))
                    set_rev(true)
                end
            elseif p == "live_to_rev" then
                if f - st.step_frame >= LIVE_HALF_PERIOD then
                    -- On a passé D frames en reverse, on swap retour à forward.
                    local i_mid = idx()
                    log(string.format("  mid-round r%d (reverse %d frames): idx=%d t=%.3fs",
                        st.round, LIVE_HALF_PERIOD, i_mid, btime()))
                    snapshot(string.format("r%02d_a_rev_mid_idx%d", st.round, i_mid))
                    st.phase = "live_to_fwd"
                    st.step_frame = f
                    set_rev(false)
                end
            elseif p == "live_to_fwd" then
                if f - st.step_frame >= LIVE_HALF_PERIOD then
                    -- Une rotation complète terminée — on est censé être revenu
                    -- à idx_baseline (ou très proche, à la tolérance près).
                    local i_end = idx()
                    local d = i_end - st.idx_baseline
                    log(string.format("  POST round r%d (fwd %d frames): idx=%d t=%.3fs drift=%+d",
                        st.round, LIVE_HALF_PERIOD, i_end, btime(), d))
                    snapshot(string.format("r%02d_b_fwd_end_idx%d", st.round, i_end))
                    table.insert(st.results,
                        { round=st.round, dir="round", idx=i_end, drift=d, t=btime() })

                    if st.round >= ROUND_TRIPS then
                        st.phase = "done"
                        st.step_frame = f
                    else
                        st.round = st.round + 1
                        st.phase = "live_to_rev"
                        st.step_frame = f
                        local i_pre = idx()
                        log(string.format("=== ROUND %d/%d swap → reverse (idx pre=%d) ===",
                            st.round, ROUND_TRIPS, i_pre))
                        set_rev(true)
                    end
                end
            end
        end

        local function tick_done(f)
            if f - st.step_frame >= 10 then
                log("")
                log("=========================================================")
                log(string.format("=== POC REVERSE MIRROR — RECAP [MODE=%s] ===", MODE))
                log("=========================================================")
                log(string.format("Baseline forward index = %d", st.idx_baseline))
                log(string.format("Round trips = %d", ROUND_TRIPS))
                if MODE == "live" then
                    log(string.format("Live half-period = %d frames", LIVE_HALF_PERIOD))
                end
                log(string.format("Tolerance = +-%d frame(s)", TOLERANCE_FRAMES))
                log("")
                log("Round | Dir   | Idx fwd | Drift | Verdict")
                log("------+-------+---------+-------+--------")
                local mx, nfail = 0, 0
                for _, r in ipairs(st.results) do
                    local v = (math.abs(r.drift) > TOLERANCE_FRAMES) and "DRIFT" or "OK"
                    if v == "DRIFT" then nfail = nfail + 1 end
                    if math.abs(r.drift) > mx then mx = math.abs(r.drift) end
                    log(string.format("  %3d | %-5s | %7d | %+5d | %s",
                        r.round, r.dir, r.idx, r.drift, v))
                end
                log("")
                log(string.format("Max |drift| = %d frame(s)", mx))
                log(string.format("Out-of-tolerance = %d / %d", nfail, #st.results))
                if nfail == 0 then
                    log(">>> RESULT: PASS — mirror frame-stable")
                    os.exit(0)
                else
                    log(">>> RESULT: FAIL — mirror dérive")
                    os.exit(1)
                end
            end
        end

        local function tick(self)
            st.frame = st.frame + 1
            local f = st.frame
            if f == 1 then
                log(string.format("controller alive — MODE=%s ROUND_TRIPS=%d", MODE, ROUND_TRIPS))
            end
            if st.phase == "done" then tick_done(f); return end
            if MODE == "static" then tick_static(f)
            else                     tick_live(f) end
        end

        local ctrl = bbfx.LuaAnimationNode("_dbg_poc_ctrl", tick)
        ctrl:addInput("dt")
        animator:addNode(ctrl)
        if tn then animator:addPort(tn, "dt", ctrl, "dt") end
        log(string.format("setup complete (MODE=%s), awaiting first tick…", MODE))
    end
}
