-- ============================================================================
-- BBFx v3.5 Lot R — Procedural content demo
-- ============================================================================
-- Exercises : bbfx.noise.generateTexture, bbfx.geometry primitives,
-- bbfx.sdf.toMesh, bbfx.fractals.mandelbrot/julia, bbfx.lsystem tree.
-- ============================================================================

print("=== BBFx Procedural Demo (Lot R) ===\n")

-- ── Noise texture ─────────────────────────────────────────────────────
local ntex = bbfx.noise.generateTexture(256, 256, { kind = "fbm", octaves = 5, seed = 42 })
print("Noise fbm texture: " .. tostring(ntex))

-- ── Geometry primitives ───────────────────────────────────────────────
print("Sphere  : " .. bbfx.geometry.createSphere("demo_sphere", 1, 16, 32))
print("Plane   : " .. bbfx.geometry.createPlane("demo_plane", 4, 4, 4, 4))
print("Cylinder: " .. bbfx.geometry.createCylinder("demo_cyl", 1, 2, 16))
print("Torus   : " .. bbfx.geometry.createTorus("demo_torus", 1, 0.3, 16, 32))

-- ── SDF smoothUnion ───────────────────────────────────────────────────
local sdfMesh = bbfx.sdf.toMesh("demo_blob",
    function(x, y, z)
        local s1 = bbfx.sdf.sphere(x, y, z, -0.5, 0, 0, 0.8)
        local s2 = bbfx.sdf.sphere(x, y, z, +0.5, 0, 0, 0.8)
        return bbfx.sdf.opSmoothUnion(s1, s2, 0.3)
    end,
    -2, -2, -2, 2, 2, 2, 24)
print("SDF blob mesh: " .. sdfMesh)

-- ── Fractals ──────────────────────────────────────────────────────────
local m = bbfx.fractals.mandelbrot(512, 512,
    { centerX = -0.75, centerY = 0.0, zoom = 1.2,
      maxIter = 200, colorScheme = "fire" })
print("Mandelbrot : " .. m)

local j = bbfx.fractals.julia(512, 512,
    { cReal = -0.8, cImag = 0.156, zoom = 1.0,
      maxIter = 200, colorScheme = "ocean" })
print("Julia      : " .. j)

-- ── L-system tree ─────────────────────────────────────────────────────
local tree = bbfx.lsystem.create({
    axiom = "F",
    rules = { F = "F[+F]F[-F]F" },
    iterations = 4,
    angle = 25.7,
    step = 1.0,
})
print("Tree mesh  : " .. tree.generateMesh("demo_tree"))

print("\nDemo ready. Attach these meshes / textures to SceneObjectNode or")
print("a material in the NodeEditor to see them live in the viewport.")
