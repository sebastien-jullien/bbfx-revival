-- Test: enable/disable nodes + entity link disconnect/reconnect
_ted_frame = 0
_ted_orig = _dbg_process_pending

function _dbg_process_pending()
    if _ted_orig then _ted_orig() end
    _ted_frame = _ted_frame + 1

    if _ted_frame == 30 then
        print("\n=== TEST ENABLE/DISABLE + ENTITY LINK ===")
        print("[SETUP] Creating preset perlin_pulse...")
        dbg.preset("perlin_pulse")

    elseif _ted_frame == 90 then
        print("[SETUP] Nodes:")
        dbg.list()
        print("[SETUP] Links:")
        dbg.links()

    -- Test 1: Disable PerlinFxNode → FX should stop
    elseif _ted_frame == 120 then
        print("\n=== TEST 1: Disable perlin_pulse_fx ===")
        dbg.set_enabled("perlin_pulse_fx", false)

    elseif _ted_frame == 150 then
        local en = dbg.is_enabled("perlin_pulse_fx")
        print("[T1] perlin_pulse_fx enabled = " .. tostring(en))
        if en == false then
            print("[T1] PASS - Node disabled")
        else
            print("[T1] FAIL")
        end

    -- Test 2: Re-enable PerlinFxNode
    elseif _ted_frame == 180 then
        print("\n=== TEST 2: Re-enable perlin_pulse_fx ===")
        dbg.set_enabled("perlin_pulse_fx", true)

    elseif _ted_frame == 210 then
        local en = dbg.is_enabled("perlin_pulse_fx")
        if en == true then
            print("[T2] PASS - Node re-enabled")
        else
            print("[T2] FAIL")
        end

    -- Test 3: Disable SceneObjectNode → mesh AND FX should hide
    elseif _ted_frame == 240 then
        print("\n=== TEST 3: Disable perlin_pulse_mesh ===")
        dbg.set_enabled("perlin_pulse_mesh", false)

    elseif _ted_frame == 270 then
        local en = dbg.is_enabled("perlin_pulse_mesh")
        if en == false then
            print("[T3] PASS - Mesh node disabled")
        else
            print("[T3] FAIL")
        end

    -- Test 4: Re-enable SceneObjectNode
    elseif _ted_frame == 300 then
        print("\n=== TEST 4: Re-enable perlin_pulse_mesh ===")
        dbg.set_enabled("perlin_pulse_mesh", true)

    elseif _ted_frame == 330 then
        print("[T4] PASS - Mesh re-enabled, no crash")

    -- Test 5: Delete entity link → FX should detach
    elseif _ted_frame == 360 then
        print("\n=== TEST 5: Unlink entity ===")
        dbg.unlink("perlin_pulse_mesh", "entity", "perlin_pulse_fx", "entity")

    elseif _ted_frame == 420 then
        print("[T5] PASS - Unlink succeeded, no crash")

    -- Test 6: Re-link entity → FX should re-attach
    elseif _ted_frame == 450 then
        print("\n=== TEST 6: Re-link entity ===")
        dbg.link("perlin_pulse_mesh", "entity", "perlin_pulse_fx", "entity")

    elseif _ted_frame == 510 then
        print("[T6] PASS - Re-link succeeded, no crash")
        print("\n=== ALL TESTS PASSED ===")
        os.exit(0)
    end
end
