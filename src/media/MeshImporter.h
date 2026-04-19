#pragma once

#include <string>

namespace bbfx {

/// v3.5 Lot Q — Assimp wrapper for external 3D model import.
///
/// Produces an OGRE ManualObject-backed mesh from any format Assimp
/// supports (OBJ / FBX / glTF / STL / DAE / 3DS / PLY ...). When Assimp
/// is not compiled in (BBFX_HAS_ASSIMP missing), `import()` returns an
/// empty string without crashing.
class MeshImporter {
public:
    /// Import `path` and return the OGRE mesh name. Empty string on
    /// failure. The mesh is cached under its canonical name so a
    /// subsequent call with the same path is a no-op.
    static std::string import(const std::string& path);

    /// Check whether the Assimp backend is available at runtime.
    static bool isAvailable();
};

} // namespace bbfx
