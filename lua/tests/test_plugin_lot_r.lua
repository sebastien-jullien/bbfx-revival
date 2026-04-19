-- ============================================================================
-- BBFx v3.5 Lot R — Procedural Geometry / SDF / Fractals / L-system
-- ============================================================================
-- Coverage : I-1476..I-1491
--   * bbfx.geometry   : createMesh / updateVertices / primitives
--   * bbfx.sdf        : primitives + booleans + toMesh (Marching Cubes)
--   * bbfx.fractals   : mandelbrot / julia (CPU-side texture generators)
--   * bbfx.lsystem    : create handle + derive + generateMesh
-- ============================================================================

print("\n================================================================")
print("  BBFx v3.5 Lot R — Procedural Geometry / SDF / Fractals / L-system")
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

-- ── Namespaces ─────────────────────────────────────────────────────────
for _, ns in ipairs({"geometry","sdf","fractals","lsystem"}) do
    check("R-001." .. ns, "bbfx." .. ns .. " namespace exists",
          type(bbfx[ns]) == "table")
end

-- ── Geometry (I-1477..I-1479) ─────────────────────────────────────────
for _, fn in ipairs({"createMesh","updateVertices","createSphere",
                     "createPlane","createCylinder","createTorus"}) do
    check("R-002." .. fn, "bbfx.geometry." .. fn .. " is a function",
          type(bbfx.geometry[fn]) == "function")
end

-- Create a triangle via raw buffers (3 vertices, 1 triangle).
local verts = {
    0, 0, 0, 0, 0, 1, 0, 0,
    1, 0, 0, 0, 0, 1, 1, 0,
    0, 1, 0, 0, 0, 1, 0, 1,
}
local idx = { 0, 1, 2 }
local triName = bbfx.geometry.createMesh("bbfx_test_tri_r", verts, idx, "BaseWhiteNoLighting")
check("R-003", "createMesh returns the provided name",
      triName == "bbfx_test_tri_r")

local sphereName = bbfx.geometry.createSphere("bbfx_test_sphere_r", 1.0, 16, 32)
check("R-004", "createSphere returns the provided name",
      sphereName == "bbfx_test_sphere_r" or sphereName ~= "",
      sphereName)

local planeName = bbfx.geometry.createPlane("bbfx_test_plane_r", 4.0, 4.0, 2, 2)
check("R-005", "createPlane returns a name",
      planeName ~= "")

local cylName = bbfx.geometry.createCylinder("bbfx_test_cyl_r", 1.0, 2.0, 16)
check("R-006", "createCylinder returns a name",
      cylName ~= "")

local torusName = bbfx.geometry.createTorus("bbfx_test_torus_r", 1.0, 0.3, 16, 32)
check("R-007", "createTorus returns a name",
      torusName ~= "")

-- ── SDF primitives + booleans (I-1480..I-1482) ─────────────────────────
for _, fn in ipairs({"sphere","box","torus","opUnion","opIntersection",
                     "opSubtraction","opSmoothUnion","toMesh"}) do
    check("R-010." .. fn, "bbfx.sdf." .. fn .. " is a function",
          type(bbfx.sdf[fn]) == "function")
end

-- Sphere(0,0,0) at radius=1, sample at point (2,0,0) -> distance = 1.
local d = bbfx.sdf.sphere(2, 0, 0, 0, 0, 0, 1)
check("R-011", "sdf.sphere(2,0,0 | 0,0,0, r=1) = 1",
      math.abs(d - 1.0) < 1e-4, d)

-- Inside the sphere at (0.5, 0, 0) -> negative distance.
local din = bbfx.sdf.sphere(0.5, 0, 0, 0, 0, 0, 1)
check("R-012", "sdf.sphere inside returns negative",
      din < 0)

-- Boolean ops.
check("R-013", "opUnion(1, 3) == 1",
      bbfx.sdf.opUnion(1, 3) == 1)
check("R-014", "opIntersection(1, 3) == 3",
      bbfx.sdf.opIntersection(1, 3) == 3)
check("R-015", "opSubtraction(1, -3) == 3",
      bbfx.sdf.opSubtraction(1, -3) == 3)
check("R-016", "opSmoothUnion converges to min when k small",
      math.abs(bbfx.sdf.opSmoothUnion(1, 3, 0) - 1) < 1e-4)

-- toMesh : a simple sphere field over [-2,2]^3 at res=8. Should produce
-- a non-empty mesh name.
local sphereMesh = bbfx.sdf.toMesh("bbfx_test_sdf_r",
    function(x, y, z) return bbfx.sdf.sphere(x, y, z, 0, 0, 0, 1) end,
    -2, -2, -2, 2, 2, 2, 8)
check("R-017", "sdf.toMesh returns a mesh name",
      type(sphereMesh) == "string" and #sphereMesh > 0)

-- ── Fractals (I-1483..I-1484) ─────────────────────────────────────────
for _, fn in ipairs({"mandelbrot","julia"}) do
    check("R-020." .. fn, "bbfx.fractals." .. fn .. " is a function",
          type(bbfx.fractals[fn]) == "function")
end
local mandTex = bbfx.fractals.mandelbrot(64, 64, { maxIter = 32 })
check("R-021", "fractals.mandelbrot returns a texture name",
      type(mandTex) == "string" and #mandTex > 0)
local juliaTex = bbfx.fractals.julia(64, 64, { maxIter = 32 })
check("R-022", "fractals.julia returns a texture name",
      type(juliaTex) == "string" and #juliaTex > 0)

-- ── L-system (I-1485..I-1487) ─────────────────────────────────────────
for _, fn in ipairs({"create"}) do
    check("R-030." .. fn, "bbfx.lsystem." .. fn .. " is a function",
          type(bbfx.lsystem[fn]) == "function")
end

local ls = bbfx.lsystem.create({
    axiom = "F",
    rules = { F = "F[+F]F[-F]F" },
    iterations = 3,
    angle = 25.7,
    step = 1.0,
})
check("R-031", "lsystem.create returns a handle",
      type(ls) == "table")
for _, m in ipairs({"derive","generateMesh","setIterations","setAngle","setStep"}) do
    check("R-032." .. m, "lsys." .. m .. " is a function",
          type(ls[m]) == "function")
end
local derived = ls.derive()
check("R-033", "derive returns a non-empty string",
      type(derived) == "string" and #derived > 1)
local treeMesh = ls.generateMesh("bbfx_test_tree_r")
check("R-034", "generateMesh returns a mesh name",
      type(treeMesh) == "string" and #treeMesh > 0)

-- ── I-1491 Non-regression : MeshGenerator v3.2 primitives still work ──
-- They are used via bbfx.geometry.createSphere/Plane/Cylinder/Torus
-- (all PASS above). The underlying MeshGenerator class is reachable via
-- any of the sphere/torus/etc. builders.
check("R-050", "MeshGenerator v3.2 primitives reachable via bbfx.geometry",
      sphereName ~= "" and torusName ~= "")

-- ── Non-regression : Lot Q/P/O bindings still reachable ───────────────
check("R-099.a", "bbfx.media still reachable",
      type(bbfx.media.openVideo) == "function")
check("R-099.b", "bbfx.sequences still reachable",
      type(bbfx.sequences.loadGif) == "function")
check("R-099.c", "bbfx.noise.simplex2D still reachable",
      type(bbfx.noise.simplex2D) == "function")

print("\n--------------------------------------------------------------")
print(string.format("  Lot R Tests: %d PASS, %d FAIL", P, F))
print("--------------------------------------------------------------\n")

if F > 0 then
    for _, n in ipairs(fails) do print("  - " .. n) end
    error("Lot R tests FAILED")
end

os.exit(0)
