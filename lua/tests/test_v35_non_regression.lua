-- v3.5 Lot A non-regression runner
-- Runs the existing dbg.test() suite once then exits so CI can assert exit 0.
print("[test_v35_non_regression] Scheduling dbg.test()...")

local frame = 0
local testNode = bbfx.LuaAnimationNode("_v35_nr_autotest", function(self)
    frame = frame + 1
    if frame == 8 then
        print("[test_v35_non_regression] Running dbg.test() (v3.4 suite)...")
        dbg.test()

        print("\n[test_v35_non_regression] --- v3.5 Lot A integration checks ---")
        -- Ensure the Lot A plugin API is wired in Studio context too.
        assert(type(bbfx.plugin) == "table", "bbfx.plugin missing in Studio")
        assert(type(dbg.plugin_scan) == "function", "dbg.plugin_scan missing")
        assert(type(dbg.plugin_list) == "function", "dbg.plugin_list missing")
        assert(type(dbg.plugin_info) == "function", "dbg.plugin_info missing")
        assert(type(dbg.plugin_validate) == "function", "dbg.plugin_validate missing")
        assert(bbfx.plugin.currentBBFxVersion() == "3.5.2", "version mismatch")
        print("[test_v35_non_regression] Lot A bindings present in Studio: OK")

        print("[test_v35_non_regression] Done — exiting.")
        os.exit(0)
    end
end)
testNode:addInput("dt")
bbfx.Animator.instance():addNode(testNode)
local rootTime = bbfx.RootTimeNode.instance()
if rootTime then
    bbfx.Animator.instance():addPort(rootTime, "dt", testNode, "dt")
end
