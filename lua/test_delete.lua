-- Test: create a SceneObjectNode then delete it — should not crash
_del_frame = 0
_del_orig_pending = _dbg_process_pending

function _dbg_process_pending()
    if _del_orig_pending then _del_orig_pending() end
    _del_frame = _del_frame + 1
    if _del_frame == 60 then
        print("[test_delete] Creating SceneObjectNode 'test_obj'...")
        dbg.create("SceneObjectNode", "test_obj")
    elseif _del_frame == 90 then
        print("[test_delete] Listing nodes before delete:")
        dbg.list()
    elseif _del_frame == 120 then
        print("[test_delete] Deleting 'test_obj'...")
        dbg.ui_delete("test_obj")
    elseif _del_frame == 150 then
        print("[test_delete] Listing nodes after delete:")
        dbg.list()
        print("[test_delete] SUCCESS — no crash!")
    elseif _del_frame == 180 then
        print("[test_delete] Creating PerlinFxNode + SceneObjectNode, linking, then deleting mesh...")
        dbg.create("SceneObjectNode", "test_mesh2")
        dbg.create("PerlinFxNode", "test_perlin")
    elseif _del_frame == 210 then
        print("[test_delete] Deleting 'test_mesh2' while PerlinFx exists...")
        dbg.ui_delete("test_mesh2")
    elseif _del_frame == 240 then
        print("[test_delete] Listing after mesh delete with FX alive:")
        dbg.list()
        print("[test_delete] ALL TESTS PASSED — no crash!")
    end
end
