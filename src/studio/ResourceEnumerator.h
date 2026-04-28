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
    static std::vector<std::string> listPresets();
    static std::vector<std::string> listTemplates();
    /// Alias for listCompositors() — semantic name for post-process effects.
    static std::vector<std::string> listPostProcessEffects();

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
    static std::vector<std::string> sPresets;
    static std::vector<std::string> sTemplates;
};

} // namespace bbfx
