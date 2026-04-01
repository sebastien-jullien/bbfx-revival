#pragma once

#include <string>
#include <vector>

namespace bbfx {

/// Enumerates available OGRE resources for Inspector dropdown population.
/// Results are cached and refreshed on demand.
class ResourceEnumerator {
public:
    static std::vector<std::string> listMeshes();
    static std::vector<std::string> listTextures();
    static std::vector<std::string> listMaterials();
    static std::vector<std::string> listParticleTemplates();
    static std::vector<std::string> listCompositors();
    static std::vector<std::string> listShaders();

    /// Invalidate all caches (call after resource reload)
    static void invalidateCache();

private:
    static bool sCacheDirty;
    static std::vector<std::string> sMeshes;
    static std::vector<std::string> sTextures;
    static std::vector<std::string> sMaterials;
    static std::vector<std::string> sParticles;
    static std::vector<std::string> sCompositors;
    static std::vector<std::string> sShaders;
};

} // namespace bbfx
