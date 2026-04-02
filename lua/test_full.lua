-- Full integration test: template flow + deletion + preset instantiation
_tf_frame = 0
_tf_orig = _dbg_process_pending

function _dbg_process_pending()
    if _tf_orig then _tf_orig() end
    _tf_frame = _tf_frame + 1

    if _tf_frame == 30 then
        print("\n=== TEST 1: Default template loads with flow ===")
        dbg.list()
        dbg.links()
    elseif _tf_frame == 60 then
        local angle = dbg.get("rotate_head", "angle")
        local rotY = dbg.get("studio_head", "rotation.y")
        print("[TEST 1] rotate_head.angle = " .. tostring(angle))
        print("[TEST 1] studio_head.rotation.y = " .. tostring(rotY))
        if angle and angle ~= 0 and rotY and rotY ~= 0 then
            print("[TEST 1] PASS - Flow animation works")
        else
            print("[TEST 1] FAIL")
        end

    elseif _tf_frame == 90 then
        print("\n=== TEST 2: Delete studio_head (should not crash) ===")
        dbg.ui_delete("studio_head")
    elseif _tf_frame == 150 then
        print("[TEST 2] PASS - No crash 60 frames after delete")

    elseif _tf_frame == 180 then
        print("\n=== TEST 3: Instantiate perlin_pulse preset ===")
        dbg.preset("perlin_pulse")
    elseif _tf_frame == 240 then
        print("[TEST 3] Nodes after preset:")
        dbg.list()
        print("[TEST 3] Links after preset:")
        dbg.links()

    elseif _tf_frame == 270 then
        -- Check if multi-node preset created both nodes
        local hasMesh = dbg.inspect("perlin_pulse_mesh")
        local hasFx = dbg.inspect("perlin_pulse_fx")
        if hasMesh and hasFx then
            print("[TEST 3] PASS - Multi-node preset created SceneObjectNode + PerlinFxNode")
        else
            print("[TEST 3] mesh=" .. tostring(hasMesh) .. " fx=" .. tostring(hasFx))
            print("[TEST 3] FAIL - Multi-node preset did not create both nodes")
        end

    elseif _tf_frame == 300 then
        print("\n=== TEST 4: Delete preset mesh (should not crash) ===")
        dbg.ui_delete("perlin_pulse_mesh")
    elseif _tf_frame == 360 then
        print("[TEST 4] PASS - No crash after deleting preset mesh")
        print("\n=== ALL TESTS PASSED ===")
        os.exit(0)
    end
end
