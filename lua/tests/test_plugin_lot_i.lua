-- ============================================================================
-- BBFx v3.5 Lot I — Ratings + Deep Links + Author Profile
-- ============================================================================
-- Coverage: I-1375..I-1384 (backend coverage)
--   - DeepLinkHandler.parse on every supported URL
--   - Invalid URLs return Unknown
--   - bbfx.deeplink.dispatch wired through PluginManager
--   - GithubReactionsFetcher cache + rating math
--   - AuthorProfilePanel is exercised by construction (UI test is Lot W)
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot I — Ratings + Deep Links + Author Profile")
print("================================================================\n")

local P, F = 0, 0
local fails = {}
local function check(id, name, cond, extra)
    if cond then
        P = P + 1; print("  PASS  " .. id .. " " .. name)
    else
        F = F + 1
        print("  FAIL  " .. id .. " " .. name .. (extra and ("  [" .. tostring(extra) .. "]") or ""))
        table.insert(fails, id .. " " .. name)
    end
end

-- ── Deep link parser ───────────────────────────────────────────────────────
check("I-001", "bbfx.deeplink namespace exists",
      type(bbfx.deeplink) == "table")
check("I-002", "parse bbfx://install/<id>",
      (function()
          local a = bbfx.deeplink.parse("bbfx://install/sjullien.plasma-wave")
          return a.kind == "Install"
             and a.pluginId == "sjullien.plasma-wave"
             and a.rawUrl == "bbfx://install/sjullien.plasma-wave"
      end)())

check("I-003", "parse bbfx://enable/<id>",
      (function()
          local a = bbfx.deeplink.parse("bbfx://enable/my.plugin")
          return a.kind == "Enable" and a.pluginId == "my.plugin"
      end)())

check("I-004", "parse bbfx://disable/<id>",
      (function()
          local a = bbfx.deeplink.parse("bbfx://disable/x.y")
          return a.kind == "Disable" and a.pluginId == "x.y"
      end)())

check("I-005", "parse bbfx://run/<id>/<type>",
      (function()
          local a = bbfx.deeplink.parse("bbfx://run/sjullien.plasma/PlasmaWave")
          return a.kind == "Run" and a.pluginId == "sjullien.plasma"
                                 and a.extra == "PlasmaWave"
      end)())

check("I-006", "parse invalid URL -> Unknown",
      (function()
          local a = bbfx.deeplink.parse("https://example.com/")
          return a.kind == "Unknown"
      end)())

check("I-007", "parse bbfx:// with no action -> Unknown",
      (function()
          local a = bbfx.deeplink.parse("bbfx://")
          return a.kind == "Unknown"
      end)())

check("I-008", "parse bbfx://run without type -> Unknown",
      (function()
          local a = bbfx.deeplink.parse("bbfx://run/only-id")
          return a.kind == "Unknown"
      end)())

-- ── dispatch is non-fatal even without handlers wired ─────────────────────
-- In bbfx.exe (headless) DeepLinkHandler has no callbacks. dispatch must
-- log and continue without crashing.
bbfx.deeplink.dispatch("bbfx://install/unknown.plugin")
check("I-009", "dispatch on headless runtime does not crash", true)

-- ── Ratings: inject + cached ──────────────────────────────────────────────
bbfx.ratings.injectForTests(42, 17, 3)
local r = bbfx.ratings.cached(42)
check("I-010", "cached(42) returns the injected reactions",
      type(r) == "table" and r.thumbsUp == 17 and r.thumbsDown == 3)
check("I-011", "injected rating = (17 / 20) * 5 == 4.25",
      math.abs((r.rating or 0) - 4.25) < 0.01,
      tostring(r.rating))

-- Unknown issue -> nil
check("I-012", "cached(unknown issue) is nil",
      bbfx.ratings.cached(99999) == nil)

-- Repo override is safe to call — no observable side effect in-process
bbfx.ratings.setRepo("mirror/community")
check("I-013", "setRepo does not crash", true)

print("\n--------------------------------------------------------------")
print(string.format("  Lot I Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot I tests FAILED")
end

os.exit(0)
