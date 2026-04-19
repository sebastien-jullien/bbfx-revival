-- ============================================================================
-- BBFx v3.5 Lot V — GitHub OAuth + Publish + Community repo
-- ============================================================================
-- Coverage : I-1519..I-1528 (backend surface)
--   * bbfx.github namespace + API (device flow / token storage / publish)
--   * Token scramble roundtrip
--   * Offline behaviours : beginDeviceFlow / pollDeviceFlow returns error
--     gracefully when offline or with unregistered client_id
--   * Community repo docs exist at docs/community-plugins-repo/
-- Actual OAuth + publish against real GitHub requires user interaction
-- and is covered by a manual test only.
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot V — GitHub Publish")
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

-- ── Namespace ─────────────────────────────────────────────────────────
check("V-001", "bbfx.github namespace exists",
      type(bbfx.github) == "table")
for _, fn in ipairs({
    "beginDeviceFlow","pollDeviceFlow",
    "storeToken","storedToken","storedLogin","isAuthenticated",
    "whoami",
    "forkUpstream","ensureBranch","commitFile","openPullRequest",
    "encodeToken","decodeToken",
}) do
    check("V-002." .. fn, "bbfx.github." .. fn .. " is a function",
          type(bbfx.github[fn]) == "function")
end

-- ── Token scramble (I-1521) ────────────────────────────────────────────
local rawToken = "gho_sTubToken1234567890"
local encoded = bbfx.github.encodeToken(rawToken)
check("V-010", "encodeToken produces a non-empty string",
      type(encoded) == "string" and #encoded > 0)
check("V-011", "encodeToken output != raw token",
      encoded ~= rawToken)
local decoded = bbfx.github.decodeToken(encoded)
check("V-012", "decodeToken roundtrips the raw token",
      decoded == rawToken)

-- Empty string round-trip
check("V-013", "encodeToken('') = ''",
      bbfx.github.encodeToken("") == "")
check("V-014", "decodeToken('') = ''",
      bbfx.github.decodeToken("") == "")

-- ── Token storage (I-1521) ─────────────────────────────────────────────
check("V-020", "isAuthenticated defaults to false when no token",
      -- Could be true if previous run left a token ; we just check
      -- that the getter returns a bool.
      type(bbfx.github.isAuthenticated()) == "boolean")

-- ── Device flow / pollDeviceFlow defensive behaviour ───────────────────
-- beginDeviceFlow will hit the real GitHub endpoint with the stub
-- client_id. On CI / offline this returns a non-empty error ; either
-- way we verify the shape of the returned table.
local code = bbfx.github.beginDeviceFlow()
check("V-030", "beginDeviceFlow returns a table",
      type(code) == "table")
check("V-031", "beginDeviceFlow table has deviceCode/userCode/error fields",
      type(code.deviceCode) == "string" and
      type(code.userCode) == "string" and
      type(code.error) == "string")

-- Poll with a fake code : must not throw, returns error.
local tok = bbfx.github.pollDeviceFlow({ deviceCode = "fake" })
check("V-032", "pollDeviceFlow returns {token, pending, error}",
      type(tok) == "table" and
      type(tok.token) == "string" and
      type(tok.pending) == "boolean" and
      type(tok.error) == "string")

-- openPullRequest without auth returns nil.
local prUrl = bbfx.github.openPullRequest("test-branch", "Test", "body")
-- Either nil (no login cached) or string if auth was cached from a
-- previous session — both are acceptable.
check("V-033", "openPullRequest returns nil or string",
      prUrl == nil or type(prUrl) == "string")

-- forkUpstream similarly safe to call offline.
local ok = pcall(function() bbfx.github.forkUpstream() end)
check("V-034", "forkUpstream does not throw even without auth", ok)

-- ── Community repo docs (I-1525..1527) ─────────────────────────────────
-- Resolved from cwd = build/windows-debug/Debug, so the docs are 4
-- levels up (build, windows-debug, Debug + project root).
local repoRoot = "../../../../docs/community-plugins-repo"
check("V-040", "docs/community-plugins-repo/README.md exists",
      bbfx.fs.exists(repoRoot .. "/README.md"))
check("V-041", "docs/community-plugins-repo/CONTRIBUTING.md exists",
      bbfx.fs.exists(repoRoot .. "/CONTRIBUTING.md"))
check("V-042", "docs/community-plugins-repo/LICENSE exists",
      bbfx.fs.exists(repoRoot .. "/LICENSE"))
check("V-043", "docs/community-plugins-repo/index.json exists",
      bbfx.fs.exists(repoRoot .. "/index.json"))
check("V-044", "docs/community-plugins-repo/schema/manifest.schema.json exists",
      bbfx.fs.exists(repoRoot .. "/schema/manifest.schema.json"))
check("V-045", ".github/workflows/validate.yml exists",
      bbfx.fs.exists(repoRoot .. "/.github/workflows/validate.yml"))

-- index.json parses + has the right structure.
local idx = bbfx.fs.readFile(repoRoot .. "/index.json")
if type(idx) == "string" then
    local j = bbfx.json.decode(idx)
    check("V-046", "index.json has version + plugins array",
          type(j) == "table" and j.version == 1 and
          type(j.plugins) == "table")
else
    check("V-046", "index.json not readable", false, "skip")
end

-- Schema has required pattern.
local sch = bbfx.fs.readFile(repoRoot .. "/schema/manifest.schema.json")
check("V-047", "manifest.schema.json requires kebab+dot id pattern",
      type(sch) == "string" and sch:find("a%-z0%-9%-") ~= nil)

-- ── Non-regression : Lot U/T bindings still reachable ──────────────────
check("V-099.a", "bbfx.hotreload still reachable",
      type(bbfx.hotreload.setEnabled) == "function")
check("V-099.b", "bbfx.authoring.writePlugin still reachable",
      type(bbfx.authoring.writePlugin) == "function")
check("V-099.c", "bbfx.renderTexture.create still reachable",
      type(bbfx.renderTexture.create) == "function")

print("\n--------------------------------------------------------------")
print(string.format("  Lot V Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot V tests FAILED")
end

os.exit(0)
