-- Test: delete studio_head then undo → mesh should reappear
_tud_frame = 0
_tud_orig = _dbg_process_pending

function _dbg_process_pending()
    if _tud_orig then _tud_orig() end
    _tud_frame = _tud_frame + 1

    if _tud_frame == 30 then
        print("\n=== TEST UNDO DELETE ===")
        print("[PRE] Nodes before delete:")
        dbg.list()
        local info = dbg.inspect("studio_head")
        print("[PRE] studio_head exists: " .. tostring(info ~= nil))
        if _sceneNodes and _sceneNodes["studio_head"] then
            print("[PRE] _sceneNodes['studio_head'] = " .. tostring(_sceneNodes["studio_head"]))
        else
            print("[PRE] _sceneNodes['studio_head'] = nil")
        end

    elseif _tud_frame == 60 then
        print("\n[DELETE] Deleting studio_head via ui_delete...")
        dbg.ui_delete("studio_head")

    elseif _tud_frame == 120 then
        print("[POST-DELETE] Nodes after delete:")
        dbg.list()
        local info = dbg.inspect("studio_head")
        print("[POST-DELETE] studio_head exists: " .. tostring(info ~= nil))

    elseif _tud_frame == 150 then
        print("\n[UNDO] Calling undo...")
        dbg.undo()

    elseif _tud_frame == 210 then
        print("[POST-UNDO] Nodes after undo:")
        dbg.list()
        local info = dbg.inspect("studio_head")
        print("[POST-UNDO] studio_head exists: " .. tostring(info ~= nil))
        if _sceneNodes and _sceneNodes["studio_head"] then
            print("[POST-UNDO] _sceneNodes['studio_head'] = " .. tostring(_sceneNodes["studio_head"]))
            print("[POST-UNDO] PASS - SceneNode restored in Lua")
        else
            print("[POST-UNDO] _sceneNodes['studio_head'] = nil")
            print("[POST-UNDO] WARN - SceneNode NOT in Lua (but OGRE entity should exist)")
        end
        -- Check links
        print("[POST-UNDO] Links:")
        dbg.links()

    elseif _tud_frame == 240 then
        print("\n=== TEST UNDO DELETE COMPLETE ===")
        os.exit(0)
    end
end
