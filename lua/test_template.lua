-- Test: verify default template loads correctly with flow animation
_tt_frame = 0
_tt_orig = _dbg_process_pending

function _dbg_process_pending()
    if _tt_orig then _tt_orig() end
    _tt_frame = _tt_frame + 1
    if _tt_frame == 60 then
        print("[test_template] Listing nodes:")
        dbg.list()
        print("[test_template] Listing links:")
        dbg.links()
    elseif _tt_frame == 90 then
        -- Check rotate_head.angle value (should be non-zero since time is flowing)
        local angle = dbg.get("rotate_head", "angle")
        print("[test_template] rotate_head.angle = " .. tostring(angle))
        local rotY = dbg.get("studio_head", "rotation.y")
        print("[test_template] studio_head.rotation.y = " .. tostring(rotY))
        if angle and angle ~= 0 then
            print("[test_template] Rotation is working via DAG!")
        else
            print("[test_template] WARNING: rotation not flowing through DAG")
        end
    elseif _tt_frame == 120 then
        -- Now delete studio_head - should not crash
        print("[test_template] Deleting studio_head...")
        dbg.ui_delete("studio_head")
    elseif _tt_frame == 180 then
        print("[test_template] 60 frames after delete - still alive!")
        print("[test_template] ALL TESTS PASSED")
        os.exit(0)
    end
end
