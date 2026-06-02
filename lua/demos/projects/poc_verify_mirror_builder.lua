-- poc_verify_mirror_builder.lua — BBFx v3.5.2 Sprint S8 Lot AV.5 round 31 (I-2056)
--
-- Vérifie OBJECTIVEMENT que reverse_stream[totalFwd-1-X] représente la même
-- source frame que forward_stream[X] pour plusieurs valeurs de X.
--
-- Si OUI (mean_abs_diff ~ 1-5 = compression noise) → le bug du décalage perçu
-- par l'utilisateur n'est PAS dans l'asset ou le mapping mathématique. Il faut
-- chercher ailleurs (timing, off-by-one dans le code engine, etc.).
--
-- Si NON (mean_abs_diff > 30 = différent source frame) → off-by-K dans
-- theora_reverse.exe ou seekToFrameIndex. À régénérer ou fix.
--
-- Run : `./bbfx-studio.exe --demo poc_verify_mirror` depuis Debug/.

return {
    name = "POC Verify Mirror Asset",
    bpm  = 120,
    description = "Vérification pixel-par-pixel forward[X] vs reverse[totalFwd-1-X] sur plusieurs frames.",
    setup = function()
        local FWD = "resources/video/bombe.ogg"
        local REV = "resources/video/bombe_reverse.ogg"

        -- Indices forward à vérifier (= positions typiques où l'user clique)
        local samples = { 10, 50, 100, 200, 305, 500, 700, 1000, 1500, 2000, 2500, 2800, 2870 }

        print("[poc_verify] === MIRROR ASSET VERIFICATION ===")
        print("[poc_verify] fwd: " .. FWD)
        print("[poc_verify] rev: " .. REV)
        print("")
        print(string.format("%-8s %-8s %-12s %-10s %-10s %s",
            "fwd_idx", "rev_idx", "mean_diff", "max_diff", "identical", "verdict"))
        print(string.rep("-", 70))

        local fails  = 0
        local maxDiff = 0
        for _, idx in ipairs(samples) do
            local r = dbg.video_verify_mirror(FWD, REV, idx)
            if r.ok then
                local verdict = "OK"
                if r.mean_abs_diff > 30 then
                    verdict = "DRIFT (off-by-K?)"
                    fails = fails + 1
                elseif r.mean_abs_diff > 10 then
                    verdict = "WARN (compression high)"
                end
                if r.mean_abs_diff > maxDiff then maxDiff = r.mean_abs_diff end
                print(string.format("%-8d %-8d %-12.3f %-10d %-10s %s",
                    r.fwd_idx, r.rev_idx, r.mean_abs_diff, r.max_abs_diff,
                    tostring(r.identical), verdict))
            else
                print(string.format("%-8d ERROR: %s", idx, r.error or "?"))
                fails = fails + 1
            end
        end

        print("")
        print(string.format("Max mean_abs_diff = %.3f", maxDiff))
        print(string.format("Out-of-tolerance  = %d / %d", fails, #samples))
        if fails == 0 then
            print(">>> RESULT: PASS — asset mirror mapping is CORRECT")
            os.exit(0)
        else
            print(">>> RESULT: FAIL — asset has off-by-K mirror mismatch")
            os.exit(1)
        end
    end
}
