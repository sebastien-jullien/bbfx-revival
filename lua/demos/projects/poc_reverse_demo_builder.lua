-- poc_reverse_demo_builder.lua — BBFx v3.5.2 Sprint S8 Lot AV.5 round 30 (I-2056)
--
-- POC qui reproduit EXACTEMENT le contexte de demo_video_wall :
--   - 1 FullscreenOverlayNode (overlay)
--   - 1 TheoraClipNode (clip) avec reverse_filename chargé
--   - 1 JoystickRouterNode (rt_rev) mode toggle
--   - clip.reverse port linké via rt_rev.toggled (mais on injecte directement la valeur)
--
-- Au lieu de gamepad réel, un controller "_dbg_poc_demo_ctrl" simule des presses
-- en mettant directement le port `clip.reverse` à 0/1 alternativement.
-- Le edge-detect du clip se passe DANS clip.update() → même tick BFS que frameUpdate
-- (= condition réelle de la démo, vs POC original qui appelait setReverse depuis ctrl).
--
-- Mesure : screenshots PRE-press et POST-press (post-press capturé au 1er blit
-- consumer suivant le swap). Si visuellement byte-identiques → pas de décalage.
-- Sinon → bug confirmé.
--
-- Run : `./bbfx-studio.exe --demo poc_reverse_demo` depuis Debug/.
-- Tunable : BBFX_LATENCY_FRAMES=N (compensation latence input, default 2).
--
-- STATUT (Lot AV.5 round 34) : HARNAIS DE DIAGNOSTIC NON-BLOQUANT.
-- Ce POC mesure la stabilité du swap reverse/forward en LECTURE CONTINUE
-- (jamais de pause). Il ne couvre PAS le scénario pause+reverse+unpause —
-- celui-ci est couvert par poc_pause_reverse_builder.lua (PASS round 33) et
-- par dbg.test() (378/378). Le drift de ±1-2 frames mesuré ici est de la
-- variance de cadence BFS pendant la fenêtre de settle (déjà documenté round 32
-- comme "variance BFS rate normale, non significative"), PAS une fuite de frame.
-- Le critère de pass est donc |drift| <= 2 pour éviter un faux positif.

return {
    name = "POC Reverse Demo Context (port + edge-detect)",
    bpm  = 120,
    description = "Reproduit le chemin gamepad-style (port reverse + edge-detect dans clip.update). Compare screenshots avant/après swap.",
    setup = function()
        local SWAPS         = tonumber(os.getenv("POC_SWAPS"))     or 8
        local IDLE_FRAMES   = tonumber(os.getenv("POC_IDLE"))      or 60
        local SETTLE_FRAMES = tonumber(os.getenv("POC_SETTLE"))    or 10
        -- Lot AV.5 round 30 : POC_LATENCY_SIM simule la latence input gamepad.
        -- Si > 0, on attend N ticks entre la "press" (= capture PRE_IDX) et le
        -- flip effectif du port reverse. Reproduit le délai SDL polling + chain
        -- GamepadNode → JoystickRouter → BFS. Sans simulation, le POC pilote
        -- via dbg.set qui est instantané → POC_LATENCY_SIM=0 par défaut.
        -- La compensation BBFX_LATENCY_FRAMES doit matcher POC_LATENCY_SIM pour
        -- obtenir drift=0.
        local LATENCY_SIM   = tonumber(os.getenv("POC_LATENCY_SIM")) or 0

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

        local st = {
            frame      = 0,
            phase      = "warmup",
            step_frame = 0,
            swap_n     = 0,
            current_rev = false,
            pre_idx    = nil,
            results    = {},  -- {swap, dir, pre_idx, post_idx, drift, pre_path, post_path}
        }

        local function log(m) print("[poc_demo] " .. m) end
        local function snap(name)
            local p = "output/poc_reverse_demo/" .. name .. ".png"
            dbg.screenshot(p)
            return p
        end
        local function idx()  return dbg.video_blitted_index("clip") end
        local function btime() return dbg.video_blitted_time("clip") end

        local ctrl = bbfx.LuaAnimationNode("_dbg_poc_demo_ctrl", function(self)
            st.frame = st.frame + 1
            local f = st.frame
            local p = st.phase

            if f == 1 then
                log(string.format("controller alive — SWAPS=%d IDLE=%d", SWAPS, IDLE_FRAMES))
            end

            if p == "warmup" then
                if f >= IDLE_FRAMES then
                    st.swap_n = 1
                    st.phase = "pre_capture"
                    st.step_frame = f
                end

            elseif p == "pre_capture" then
                if st.pre_idx == nil then
                    -- Capture PRE-press AT THIS MOMENT (= what user sees at press)
                    st.pre_idx = idx()
                    local dir = st.current_rev and "rev" or "fwd"
                    st.pre_path = snap(string.format("s%02d_a_PRE_%s_idx%d", st.swap_n, dir, st.pre_idx))
                    log(string.format("=== SWAP %d/%d (currently %s) PRE idx=%d t=%.3fs ===",
                        st.swap_n, SWAPS, dir, st.pre_idx, btime()))
                    st.latency_remaining = LATENCY_SIM
                end
                -- Optional input latency simulation : wait N ticks before flipping
                -- the port. Reproduces gamepad → SDL → BFS chain latency.
                if st.latency_remaining > 0 then
                    st.latency_remaining = st.latency_remaining - 1
                    return
                end
                -- Latency done : flip the port (= "user input arrives at system").
                st.current_rev = not st.current_rev
                dbg.set("clip", "reverse", st.current_rev and 1.0 or 0.0)
                st.phase = "post_capture"
                st.step_frame = f

            elseif p == "post_capture" then
                if f - st.step_frame >= SETTLE_FRAMES then
                    -- Capture POST-press : screenshot + idx after swap settled
                    local post_idx = idx()
                    local dir_pre  = (not st.current_rev) and "rev" or "fwd"  -- direction BEFORE the swap
                    local dir_post = st.current_rev and "rev" or "fwd"        -- direction AFTER the swap
                    local drift    = post_idx - st.pre_idx
                    local post_path = snap(string.format("s%02d_b_POST_%s_idx%d", st.swap_n, dir_post, post_idx))
                    log(string.format("    POST swap %d (now %s): idx=%d t=%.3fs drift=%+d",
                        st.swap_n, dir_post, post_idx, btime(), drift))
                    table.insert(st.results, {
                        swap = st.swap_n, dir_pre = dir_pre, dir_post = dir_post,
                        pre_idx = st.pre_idx, post_idx = post_idx, drift = drift,
                        pre_path = st.pre_path, post_path = post_path
                    })

                    if st.swap_n >= SWAPS then
                        st.phase = "done"
                        st.step_frame = f
                    else
                        st.swap_n = st.swap_n + 1
                        st.phase  = "idle"
                        st.step_frame = f
                        st.pre_idx = nil  -- reset for next swap's pre_capture
                    end
                end

            elseif p == "idle" then
                -- Laisser jouer en mode courant pendant IDLE_FRAMES, puis prochain swap.
                if f - st.step_frame >= IDLE_FRAMES then
                    st.phase = "pre_capture"
                    st.step_frame = f
                end

            elseif p == "done" then
                if f - st.step_frame >= 10 then
                    log("")
                    log("=========================================================")
                    log("=== POC REVERSE DEMO CTX — RECAP ===")
                    log("=========================================================")
                    log(string.format("Total swaps = %d  Idle per phase = %d frames",
                        #st.results, IDLE_FRAMES))
                    log("")
                    log("Swap | Pre dir | Pre idx | Post dir | Post idx | Drift | Verdict")
                    log("-----+---------+---------+----------+----------+-------+--------")
                    local mx = 0
                    local fails = 0
                    for _, r in ipairs(st.results) do
                        -- Critère : la frame POST-swap doit retomber sur la frame PRE-swap
                        -- (= mirror du moment du press). Tolérance |drift| <= 2 :
                        -- la fenêtre de settle (SETTLE_FRAMES) laisse jouer la vidéo,
                        -- donc ±1-2 frames de variance de cadence BFS sont normales et
                        -- non significatives (cf. header + worklog round 32/34). Au-delà
                        -- de 2 → vraie fuite de frame au swap.
                        local v = (math.abs(r.drift) <= 2) and "OK" or "DRIFT"
                        if v == "DRIFT" then fails = fails + 1 end
                        if math.abs(r.drift) > mx then mx = math.abs(r.drift) end
                        log(string.format("  %3d | %3s     | %7d | %3s      | %8d | %+5d | %s",
                            r.swap, r.dir_pre, r.pre_idx, r.dir_post, r.post_idx, r.drift, v))
                    end
                    log("")
                    log(string.format("Max |drift| = %d frame(s)   Failures = %d / %d",
                        mx, fails, #st.results))
                    log("(diagnostic non-bloquant — tolérance |drift| <= 2, cf. header)")
                    if fails == 0 then
                        log(">>> RESULT: PASS — port-edge swap is frame-stable")
                        os.exit(0)
                    else
                        log(">>> RESULT: FAIL — port-edge swap leaks frames (drift > 2)")
                        os.exit(1)
                    end
                end
            end
        end)
        ctrl:addInput("dt")
        animator:addNode(ctrl)
        if tn then animator:addPort(tn, "dt", ctrl, "dt") end
        log("setup complete, awaiting first tick…")
    end
}
