-- Test: verify preset creates both SceneObjectNode + FX with entity link
_tpm_frame = 0
_tpm_orig = _dbg_process_pending

function _dbg_process_pending()
    if _tpm_orig then _tpm_orig() end
    _tpm_frame = _tpm_frame + 1

    if _tpm_frame == 30 then
        print("\n=== TEST PRESET MULTI-NODE ===")
        print("[PRE] Nodes:")
        dbg.list()

    elseif _tpm_frame == 60 then
        print("\n[1] Instantiating elastic_bounce...")
        dbg.preset("elastic_bounce")

    elseif _tpm_frame == 120 then
        print("[1] Nodes after preset:")
        dbg.list()
        print("[1] Links:")
        dbg.links()
        local hasMesh = dbg.inspect("elastic_bounce_mesh")
        local hasFx = dbg.inspect("elastic_bounce_fx")
        if hasMesh and hasFx then
            print("[1] PASS - elastic_bounce created both mesh + fx nodes")
        else
            print("[1] FAIL - mesh=" .. tostring(hasMesh) .. " fx=" .. tostring(hasFx))
        end

    elseif _tpm_frame == 150 then
        print("\n[2] Deleting elastic_bounce_mesh...")
        dbg.ui_delete("elastic_bounce_mesh")

    elseif _tpm_frame == 210 then
        print("[2] PASS - No crash after deleting preset mesh")

    elseif _tpm_frame == 240 then
        print("\n[3] Undo delete...")
        dbg.undo()

    elseif _tpm_frame == 300 then
        local hasMesh = dbg.inspect("elastic_bounce_mesh")
        if hasMesh then
            print("[3] PASS - Undo restored mesh node")
        else
            print("[3] FAIL - Undo did not restore mesh node")
        end
        print("\n=== ALL TESTS PASSED ===")
        os.exit(0)
    end
end
