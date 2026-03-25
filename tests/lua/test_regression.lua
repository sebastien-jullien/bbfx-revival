-- test_regression.lua
-- Headless regression tests for ogre-lua bindings (no Engine/OGRE window required)
-- Run via: test_runner --headless tests/lua/test_regression.lua

local pass = 0
local fail = 0
local total = 0

local function assert_test(name, condition, msg)
    total = total + 1
    if condition then
        pass = pass + 1
        print(string.format("  PASS  %s", name))
    else
        fail = fail + 1
        print(string.format("  FAIL  %s — %s", name, msg or "assertion failed"))
    end
end

local function approx(a, b, eps)
    eps = eps or 0.001
    return math.abs(a - b) < eps
end

print("=== test_regression.lua ===")
print("")

-- ── 1. Vector3 basics ──────────────────────────────────────────────────────
print("[Vector3]")
local v = Ogre.Vector3(1, 2, 3)
assert_test("Vector3 construct", v.x == 1 and v.y == 2 and v.z == 3)

local v2 = Ogre.Vector3(4, 5, 6)
local vsum = v + v2
assert_test("Vector3 add", vsum.x == 5 and vsum.y == 7 and vsum.z == 9)

assert_test("Vector3 length", approx(Ogre.Vector3(3, 4, 0):length(), 5.0))

assert_test("Vector3.UNIT_Y", Ogre.Vector3.UNIT_Y.y == 1 and Ogre.Vector3.UNIT_Y.x == 0)

local vn = Ogre.Vector3(0, 0, 5):normalisedCopy()
assert_test("Vector3 normalisedCopy", approx(vn.z, 1.0) and approx(vn.x, 0.0))

-- ── 2. Quaternion basics ───────────────────────────────────────────────────
print("[Quaternion]")
local q = Ogre.Quaternion.IDENTITY
assert_test("Quaternion IDENTITY", approx(q.w, 1.0) and approx(q.x, 0.0))

local rotated = q * Ogre.Vector3.UNIT_Y
assert_test("Quaternion * Vector3", approx(rotated.y, 1.0))

-- ── 3. ColourValue ─────────────────────────────────────────────────────────
print("[ColourValue]")
assert_test("ColourValue.White", Ogre.ColourValue.White.r == 1 and Ogre.ColourValue.White.g == 1)

local c = Ogre.ColourValue(1, 0.5, 0.25, 1)
assert_test("ColourValue construct", approx(c.g, 0.5) and approx(c.b, 0.25))

-- ── 4. Radian / Degree ────────────────────────────────────────────────────
print("[Radian/Degree]")
assert_test("Radian to Degree", approx(Ogre.Radian(math.pi):valueDegrees(), 180))
assert_test("Degree to Radian", approx(Ogre.Degree(90):valueRadians(), math.pi / 2))

-- ── 5. SceneNode operations (headless SceneManager) ───────────────────────
print("[SceneNode]")
local root_node = scene:getRootSceneNode()
assert_test("RootSceneNode exists", root_node ~= nil)

local child = root_node:createChildSceneNode("RegChild1")
assert_test("createChildSceneNode", child:getName() == "RegChild1")

child:setPosition(Ogre.Vector3(10, 20, 30))
local pos = child:getPosition()
assert_test("SceneNode setPosition/getPosition", pos.x == 10 and pos.y == 20 and pos.z == 30)

-- ── 6. Light creation ──────────────────────────────────────────────────────
print("[Light]")
local light = scene:createLight("RegTestLight")
assert_test("Light created", light ~= nil)
light:setType(Ogre.Light.LT_POINT)
light:setDiffuseColour(Ogre.ColourValue(1, 1, 1))
assert_test("Light setType/setDiffuseColour", true) -- no crash = pass

-- ── 7. Camera creation ────────────────────────────────────────────────────
print("[Camera]")
local cam = scene:createCamera("RegTestCam")
assert_test("Camera created", cam ~= nil)
cam:setNearClipDistance(0.1)
cam:setFarClipDistance(1000)
assert_test("Camera clip distances", true) -- no crash = pass

-- ── 8. AnyNumeric roundtrip ───────────────────────────────────────────────
print("[AnyNumeric]")
local a = Ogre.make_any_float(3.14)
local result = Ogre.any_cast_float(a)
assert_test("AnyNumeric roundtrip", approx(result, 3.14))

-- ── Summary ────────────────────────────────────────────────────────────────
print("")
print(string.format("=== %d/%d tests passed ===", pass, total))

if fail > 0 then
    os.exit(1)
else
    os.exit(0)
end
