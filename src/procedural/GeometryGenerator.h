#pragma once

#include <string>
#include <vector>

namespace bbfx {

/// v3.5 Lot R — Lua-facing procedural mesh builder.
///
/// Thin wrapper on top of OGRE's ManualObject that accepts raw vertex
/// and index buffers from Lua. Vertices are packed as
/// (x, y, z, nx, ny, nz, u, v) — 8 floats per vertex. Indices are
/// 32-bit unsigned integers (OGRE converts to 16-bit when possible).
///
/// Primitive helpers (sphere/plane/cylinder/torus) delegate to the
/// existing MeshGenerator so the two APIs share generation code.
class GeometryGenerator {
public:
    /// Create a new mesh from raw buffers. `vertices.size()` must be a
    /// multiple of 8; `indices` are triangle list. Material name defaults
    /// to "BaseWhiteNoLighting" when empty. Returns the created mesh name
    /// (same as `name`) on success, empty on failure.
    static std::string createMesh(const std::string& name,
                                       const std::vector<float>& vertices,
                                       const std::vector<uint32_t>& indices,
                                       const std::string& material);

    /// Replace an existing mesh's vertices in place. Must be called with
    /// a buffer matching the mesh's vertex count. Returns false if the
    /// mesh does not exist or buffer sizes mismatch.
    static bool updateVertices(const std::string& meshName,
                                  const std::vector<float>& vertices);

    /// Convenience helpers — register a mesh in MeshManager under `name`
    /// and return `name` on success, empty string on failure.
    static std::string createSphere  (const std::string& name, float radius,
                                          int rings, int segments);
    static std::string createPlane   (const std::string& name, float w, float h,
                                          int segX, int segY);
    static std::string createCylinder(const std::string& name, float radius,
                                          float height, int segments);
    static std::string createTorus   (const std::string& name,
                                          float majorRadius, float minorRadius,
                                          int rings, int segments);
};

} // namespace bbfx
