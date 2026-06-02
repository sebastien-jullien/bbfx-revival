-- poc_verify_seek_builder.lua — round 31 (I-2056) — vérifie seekToFrameIndex
-- Si seek + decode ≠ sequential decode → seekToFrameIndex est bugué.

return {
    name = "POC Verify SeekToFrameIndex",
    bpm  = 120,
    setup = function()
        local files = {
            "resources/video/bombe.ogg",
            "resources/video/bombe_reverse.ogg",
        }
        local samples = { 100, 200, 305, 500, 700, 1000, 1500, 2000, 2500, 2800 }

        print("[poc_seek] === SEEK vs SEQUENTIAL DECODE COMPARISON ===")
        print("")
        local fails = 0
        local total = 0
        for _, file in ipairs(files) do
            print("File: " .. file)
            print(string.format("%-8s %-12s %-10s %-10s %s",
                "idx", "mean_diff", "max_diff", "identical", "verdict"))
            print(string.rep("-", 60))
            for _, idx in ipairs(samples) do
                total = total + 1
                local r = dbg.video_verify_seek(file, idx)
                if r.ok then
                    local verdict
                    if r.identical then
                        verdict = "EXACT (seek == sequential)"
                    elseif r.mean_diff < 0.5 then
                        verdict = "OK (negligible diff)"
                    elseif r.mean_diff < 5 then
                        verdict = "WARN (small diff)"
                    else
                        verdict = "DRIFT (seek bug!)"
                        fails = fails + 1
                    end
                    print(string.format("%-8d %-12.4f %-10d %-10s %s",
                        idx, r.mean_diff, r.max_diff,
                        tostring(r.identical), verdict))
                else
                    print(string.format("%-8d ERROR: %s", idx, r.error or "?"))
                    fails = fails + 1
                end
            end
            print("")
        end
        print(string.format("Failures = %d / %d", fails, total))
        if fails == 0 then
            print(">>> RESULT: seekToFrameIndex matches sequential decode")
            os.exit(0)
        else
            print(">>> RESULT: seekToFrameIndex DIVERGES from sequential decode")
            os.exit(1)
        end
    end
}
