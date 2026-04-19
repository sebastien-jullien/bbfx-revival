-- ============================================================================
-- BBFx v3.5 Lot W — Example plugins + docs + audit final
-- ============================================================================
-- Coverage : I-1529..I-1539
--   * 3 example plugins exist and pass the manifest validator
--   * docs/plugin-api.md + authoring/sandbox/gamepad guides exist
--   * Global non-regression suite (all prior lots still PASS)
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot W — Example plugins + Docs + Final audit")
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

-- ── 3 example plugins (I-1529..I-1531) ─────────────────────────────────
local projectRoot = "../../.."   -- from build/windows-debug/Debug/lua/plugins view
local plugins = {
    "example-plasma-wave",
    "example-sdf-raymarch",
    "example-lsystem-tree",
}

for _, id in ipairs(plugins) do
    local dir = "lua/plugins/" .. id
    check("W-001." .. id, id .. "/manifest.json exists",
          bbfx.fs.exists(dir .. "/manifest.json"))
    check("W-002." .. id, id .. "/init.lua exists",
          bbfx.fs.exists(dir .. "/init.lua"))
    check("W-003." .. id, id .. "/README.md exists",
          bbfx.fs.exists(dir .. "/README.md"))

    local mf = bbfx.fs.readFile(dir .. "/manifest.json")
    if type(mf) == "string" then
        local j = bbfx.json.decode(mf)
        check("W-004." .. id, id .. " manifest.id matches validator pattern",
              type(j.id) == "string" and bbfx.authoring.isValidId(j.id))
        check("W-005." .. id, id .. " manifest.version semver",
              type(j.version) == "string" and j.version:match("^%d+%.%d+%.%d+$") ~= nil)
        check("W-006." .. id, id .. " manifest.author.name present",
              type(j.author) == "table" and #j.author.name > 0)
    else
        check("W-004." .. id, "manifest readable", false)
    end

    local res = bbfx.authoring.validatePath(dir)
    check("W-010." .. id, id .. " passes validator",
          res.ok == true,
          (not res.ok) and table.concat(res.errors, "; ") or "")

    local chunk, err = loadfile(dir .. "/init.lua")
    check("W-011." .. id, id .. "/init.lua parses",
          type(chunk) == "function", err)
end

-- ── CHANGELOG.md for plasma-wave + sdf-raymarch (I-1529, I-1530)
check("W-020", "example-plasma-wave/CHANGELOG.md exists",
      bbfx.fs.exists("lua/plugins/example-plasma-wave/CHANGELOG.md"))
check("W-021", "example-sdf-raymarch/CHANGELOG.md exists",
      bbfx.fs.exists("lua/plugins/example-sdf-raymarch/CHANGELOG.md"))

-- ── Shader resource for plasma-wave (I-1529) ──────────────────────────
check("W-022", "example-plasma-wave/shaders/plasma_wave.frag exists",
      bbfx.fs.exists("lua/plugins/example-plasma-wave/shaders/plasma_wave.frag"))
check("W-023", "example-sdf-raymarch/shaders/sdf_raymarch.frag exists",
      bbfx.fs.exists("lua/plugins/example-sdf-raymarch/shaders/sdf_raymarch.frag"))

-- ── Documentation files (I-1534, I-1535) ──────────────────────────────
local docsRoot = "../../../../docs"
local docs = {
    "plugin-api.md",
    "plugin-authoring-guide.md",
    "sandbox-security.md",
    "gamepad-mapping-guide.md",
}
for _, d in ipairs(docs) do
    check("W-030." .. d, "docs/" .. d .. " exists",
          bbfx.fs.exists(docsRoot .. "/" .. d))
end

-- ── Non-regression : key bindings from every lot still reachable ─────
for _, pair in ipairs({
    {"plugin",        "list"},                  -- Lot A
    {"plugin",        "install"},               -- Lot F
    {"community",     "size"},                  -- Lot H
    {"gamepad",       "count"},                 -- Lot J
    {"gamepadMapping","loadFile"},              -- Lot K
    {"midi",          "getCC"},                 -- Lot M
    {"osc",           "send"},                  -- Lot M
    {"artnet",        "send"},                  -- Lot M
    {"textureShare",  "backend"},               -- Lot M
    {"noise",         "simplex2D"},             -- Lot N
    {"easing",        "linear"},                -- Lot N
    {"tempo",         "getBPM"},                -- Lot N
    {"timeline",      "create"},                -- Lot N
    {"http",          "get"},                   -- Lot O
    {"fs",            "readFile"},              -- Lot O
    {"json",          "encode"},                -- Lot O
    {"media",         "openVideo"},             -- Lot Q
    {"images",        "load"},                  -- Lot Q
    {"sequences",     "loadGif"},               -- Lot Q
    {"models",        "import"},                -- Lot Q
    {"geometry",      "createMesh"},            -- Lot R
    {"sdf",           "sphere"},                -- Lot R
    {"fractals",      "mandelbrot"},            -- Lot R
    {"lsystem",       "create"},                -- Lot R
    {"authoring",     "writePlugin"},           -- Lot S
    {"renderTexture", "create"},                -- Lot T
    {"frameBuffer",   "saveToFile"},            -- Lot T
    {"compositor",    "enable"},                -- Lot T
    {"hotreload",     "tick"},                  -- Lot U
    {"github",        "beginDeviceFlow"},       -- Lot V
}) do
    local ns, fn = pair[1], pair[2]
    check("W-099." .. ns, "bbfx." .. ns .. "." .. fn .. " still callable",
          type(bbfx[ns]) == "table" and type(bbfx[ns][fn]) == "function")
end

print("\n--------------------------------------------------------------")
print(string.format("  Lot W Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot W tests FAILED")
end

os.exit(0)
