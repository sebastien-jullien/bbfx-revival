-- Test: delete the default studio_head SceneObjectNode
-- This should NOT crash even though rotate_head caches a pointer to it
_tdh_frame = 0
_tdh_orig = _dbg_process_pending

function _dbg_process_pending()
    if _tdh_orig then _tdh_orig() end
    _tdh_frame = _tdh_frame + 1
    if _tdh_frame == 60 then
        print("[test_delete_head] Listing nodes before delete:")
        dbg.list()
        print("[test_delete_head] _rotateTarget = " .. tostring(_rotateTarget))
        print("[test_delete_head] _sceneNodes = " .. tostring(_sceneNodes))
        if _sceneNodes then
            for k,v in pairs(_sceneNodes) do
                print("  _sceneNodes[" .. k .. "] = " .. tostring(v))
            end
        end
    elseif _tdh_frame == 90 then
        print("[test_delete_head] Deleting 'studio_head'...")
        dbg.ui_delete("studio_head")
    elseif _tdh_frame == 120 then
        print("[test_delete_head] 30 frames after delete - still alive!")
        print("[test_delete_head] _rotateTarget = " .. tostring(_rotateTarget))
        dbg.list()
    elseif _tdh_frame == 180 then
        print("[test_delete_head] 90 frames after delete - SUCCESS!")
        os.exit(0)
    end
end
