-- Quick delete test: create SceneObjectNode, delete it, check for crash
_tdq_frame = 0
_tdq_orig = _dbg_process_pending

function _dbg_process_pending()
    if _tdq_orig then _tdq_orig() end
    _tdq_frame = _tdq_frame + 1
    if _tdq_frame == 30 then
        print("[test] Creating SceneObjectNode 'test_obj'...")
        dbg.create("SceneObjectNode", "test_obj")
    elseif _tdq_frame == 60 then
        print("[test] Nodes before delete:")
        dbg.list()
    elseif _tdq_frame == 90 then
        print("[test] Deleting 'test_obj' via ui_delete...")
        dbg.ui_delete("test_obj")
    elseif _tdq_frame == 120 then
        print("[test] Nodes after delete:")
        dbg.list()
        print("[test] SUCCESS - no crash after SceneObjectNode deletion!")
    elseif _tdq_frame == 150 then
        print("[test] EXITING")
        os.exit(0)
    end
end
