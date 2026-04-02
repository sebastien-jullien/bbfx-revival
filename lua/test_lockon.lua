-- Lock-on orbit test: deferred to run after engine is ready
-- The _dbg_process_pending function runs each frame, so we use a frame counter

_lockon_frame = 0
_lockon_orig_pending = _dbg_process_pending

function _dbg_process_pending()
    if _lockon_orig_pending then _lockon_orig_pending() end
    _lockon_frame = _lockon_frame + 1
    -- Wait 60 frames for everything to initialize
    if _lockon_frame == 60 then
        print("[test] Creating test scene...")
        dbg.create("SceneObjectNode", "test_sphere")
    elseif _lockon_frame == 90 then
        print("[test] Running lockon_test...")
        dbg.lockon_test()
        print("[test] Done! Check screenshots in Debug/")
    end
end
