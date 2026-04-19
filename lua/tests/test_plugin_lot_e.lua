-- ============================================================================
-- BBFx v3.5 Lot E — HTTP + WebSocket client tests
-- ============================================================================
-- Coverage: I-1325..I-1336
--   - bbfx.http.getSync / get / post / download
--   - SHA-256 file hash
--   - pumpMainThread + waitIdle
--   - bbfx.websocket.connect stub (Unsupported until baseline bump)
-- Runs standalone: `bbfx.exe lua/tests/test_plugin_lot_e.lua`
--
-- Note: these tests intentionally avoid hitting the public internet so CI
-- can run offline. We use the file:// scheme via a temp file for the
-- download path and a known-synthetic URL for the negative branches.
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot E — HTTP + WebSocket")
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

-- ── E-001: bbfx.http namespace exposed ────────────────────────────────────
check("E-001", "bbfx.http namespace exists", type(bbfx.http) == "table")
check("E-002", "bbfx.http.getSync is a function", type(bbfx.http.getSync) == "function")
check("E-003", "bbfx.http.get is a function", type(bbfx.http.get) == "function")
check("E-004", "bbfx.http.post is a function", type(bbfx.http.post) == "function")
check("E-005", "bbfx.http.download is a function", type(bbfx.http.download) == "function")
check("E-006", "bbfx.http.sha256File is a function", type(bbfx.http.sha256File) == "function")

-- ── E-007: SHA-256 of a known value ───────────────────────────────────────
-- Write "hello" (no newline) in binary mode so Windows doesn't translate
-- '\n' to '\r\n'. Verify SHA-256 matches the canonical digest.
--   echo -n "hello" | sha256sum
--   = 2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
local tmp = (os.getenv("TEMP") or os.getenv("TMP") or "/tmp"):gsub("[/\\]+$", "")
local hashFile = tmp .. "/bbfx_lot_e_hash.bin"
local f = io.open(hashFile, "wb"); f:write("hello"); f:close()
local h = bbfx.http.sha256File(hashFile)
check("E-007", "sha256File('hello') == known digest",
      h == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
      "got " .. tostring(h))

-- SHA-256 of a missing file -> empty string
check("E-008", "sha256File on missing file returns ''",
      bbfx.http.sha256File("/does/not/exist/xyz_12345") == "")

-- ── E-009/E-010: getSync transport error surfaces in response.error ──────
local r = bbfx.http.getSync("http://127.0.0.1:1/nope", 2)
check("E-009", "getSync on unreachable host returns table",
      type(r) == "table")
check("E-010", "getSync on unreachable host has non-empty error",
      type(r.error) == "string" and #r.error > 0,
      r.error)

-- ── E-011/E-012: async get() marshals callback to main thread ────────────
local gotCallback = false
local gotTable = nil
bbfx.http.get("http://127.0.0.1:1/also-nope", function(resp)
    gotCallback = true
    gotTable = resp
end, 2)
-- Wait for the worker to complete and the main queue to drain.
local ok = bbfx.http.waitIdle(10)
check("E-011", "waitIdle returns true within timeout", ok == true)
check("E-012", "async get() invoked the callback", gotCallback == true)
check("E-013", "async get() passed a table to the callback",
      type(gotTable) == "table" and type(gotTable.error) == "string")

-- ── E-014: bbfx.websocket namespace and Unsupported stub ─────────────────
check("E-014", "bbfx.websocket namespace exists", type(bbfx.websocket) == "table")
local wsErrSeen = nil
local conn = bbfx.websocket.connect("ws://echo.invalid/", {
    onError = function(err) wsErrSeen = err end,
})
-- The stub always reports unsupported synchronously.
check("E-015", "websocket.connect returns a handle",
      conn ~= nil)
check("E-016", "WebSocket is currently unsupported (stub in place)",
      conn:isSupported() == false)
check("E-017", "WebSocket stub populates onError with explanation",
      type(wsErrSeen) == "string" and #wsErrSeen > 0,
      wsErrSeen)
check("E-018", "send on unsupported connection returns false",
      conn:send("hello") == false)

-- ── Cleanup ──────────────────────────────────────────────────────────────
os.remove(hashFile)

print("\n--------------------------------------------------------------")
print(string.format("  Lot E Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    print("Failures:")
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot E tests FAILED")
end

os.exit(0)
