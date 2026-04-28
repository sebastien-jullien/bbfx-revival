#include "ResourceEnumerator.h"
#include <OgreMeshManager.h>
#include <OgreMaterialManager.h>
#include <OgreTextureManager.h>
#include <OgreTexture.h>
#include <OgreResourceGroupManager.h>
#include <OgreParticleSystemManager.h>
#include <OgreCompositorManager.h>
#include <algorithm>
#include <filesystem>

namespace bbfx {

bool ResourceEnumerator::sCacheDirty = true;
std::vector<std::string> ResourceEnumerator::sMeshes;
std::vector<std::string> ResourceEnumerator::sTextures;
std::vector<std::string> ResourceEnumerator::sMaterials;
std::vector<std::string> ResourceEnumerator::sParticles;
std::vector<std::string> ResourceEnumerator::sCompositors;
std::vector<std::string> ResourceEnumerator::sShaders;
std::vector<std::string> ResourceEnumerator::sPresets;
std::vector<std::string> ResourceEnumerator::sTemplates;

void ResourceEnumerator::invalidateCache() { sCacheDirty = true; }

static std::vector<std::string> scanResourceGroup(const std::string& pattern) {
    std::vector<std::string> result;
    auto& rgm = Ogre::ResourceGroupManager::getSingleton();
    auto files = rgm.findResourceNames("General", pattern);
    for (auto& f : *files) result.push_back(f);

    // v3.5 Lot C: also scan every per-plugin resource group ("Plugin_<id>")
    // so AssetBrowser shows shaders/textures/materials contributed by
    // enabled plugins.
    for (const auto& g : rgm.getResourceGroups()) {
        if (g.rfind("Plugin_", 0) != 0) continue;
        if (!rgm.isResourceGroupInitialised(g)) continue;
        auto pluginFiles = rgm.findResourceNames(g, pattern);
        for (auto& f : *pluginFiles) result.push_back(f);
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<std::string> ResourceEnumerator::listMeshes() {
    if (sCacheDirty || sMeshes.empty()) {
        sMeshes = scanResourceGroup("*.mesh");
        // Also include procedural meshes registered in MeshManager
        auto it = Ogre::MeshManager::getSingleton().getResourceIterator();
        while (it.hasMoreElements()) {
            auto res = it.peekNextValue();
            if (res) {
                std::string name = res->getName();
                if (std::find(sMeshes.begin(), sMeshes.end(), name) == sMeshes.end()) {
                    sMeshes.push_back(name);
                }
            }
            it.moveNext();
        }
        std::sort(sMeshes.begin(), sMeshes.end());
    }
    return sMeshes;
}

std::vector<std::string> ResourceEnumerator::listTextures() {
    if (sCacheDirty || sTextures.empty()) {
        sTextures.clear();
        for (auto& ext : {"*.png", "*.jpg", "*.tga", "*.dds", "*.bmp"}) {
            auto files = scanResourceGroup(ext);
            sTextures.insert(sTextures.end(), files.begin(), files.end());
        }
        std::sort(sTextures.begin(), sTextures.end());
        sTextures.erase(std::unique(sTextures.begin(), sTextures.end()), sTextures.end());
    }
    return sTextures;
}

std::vector<std::string> ResourceEnumerator::listMaterials() {
    if (sCacheDirty || sMaterials.empty()) {
        sMaterials.clear();
        auto it = Ogre::MaterialManager::getSingleton().getResourceIterator();
        while (it.hasMoreElements()) {
            auto res = it.peekNextValue();
            if (res) sMaterials.push_back(res->getName());
            it.moveNext();
        }
        std::sort(sMaterials.begin(), sMaterials.end());
    }
    return sMaterials;
}

std::vector<std::string> ResourceEnumerator::listParticleTemplates() {
    if (sCacheDirty || sParticles.empty()) {
        sParticles.clear();
        auto it = Ogre::ParticleSystemManager::getSingleton().getTemplateIterator();
        while (it.hasMoreElements()) {
            sParticles.push_back(it.peekNextKey());
            it.moveNext();
        }
        std::sort(sParticles.begin(), sParticles.end());
    }
    return sParticles;
}

std::vector<std::string> ResourceEnumerator::listCompositors() {
    if (sCacheDirty || sCompositors.empty()) {
        sCompositors.clear();
        auto it = Ogre::CompositorManager::getSingleton().getResourceIterator();
        while (it.hasMoreElements()) {
            auto res = it.peekNextValue();
            if (res) sCompositors.push_back(res->getName());
            it.moveNext();
        }
        std::sort(sCompositors.begin(), sCompositors.end());
    }
    return sCompositors;
}

static bool isBBFxShader(const std::string& name) {
    // Filter out OGRE internal / compositor shaders — keep only BBFx effect shaders
    static const char* internalPrefixes[] = {
        "SGXLib_", "RTSLib_", "RTSS",
        "Shadow", "StdQuad_", "GLSL_GL3",
        "Ogre", "OldTV", "Glass",
        "selection_highlight",
        "BlackAndWhite", "Blur", "DOF_",
        "Embossed", "FFPLib_", "FFPTransform",
        "Laplace", "Tiling", "HeatVision",
        "Invert_", "Posterize_",
        "ASCII_", "Halftone_"
    };
    for (auto* prefix : internalPrefixes) {
        if (name.find(prefix) == 0) return false;
    }
    return true;
}

std::vector<std::string> ResourceEnumerator::listShaders() {
    if (sCacheDirty || sShaders.empty()) {
        sShaders.clear();
        for (auto& ext : {"*.frag", "*.vert", "*.glsl"}) {
            auto files = scanResourceGroup(ext);
            for (auto& f : files) {
                if (isBBFxShader(f)) sShaders.push_back(f);
            }
        }
        std::sort(sShaders.begin(), sShaders.end());
        sShaders.erase(std::unique(sShaders.begin(), sShaders.end()), sShaders.end());
    }
    sCacheDirty = false;
    return sShaders;
}

static std::vector<std::string> scanLuaDir(const std::string& dir) {
    std::vector<std::string> result;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return result;
    for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".lua") continue;
        std::string name = entry.path().stem().string();
        if (!name.empty() && name[0] != '_') result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> ResourceEnumerator::listPresets() {
    if (sPresets.empty()) {
        sPresets = scanLuaDir("lua/presets");
    }
    return sPresets;
}

std::vector<std::string> ResourceEnumerator::listTemplates() {
    if (sTemplates.empty()) {
        sTemplates = scanLuaDir("lua/templates");
    }
    return sTemplates;
}

std::vector<std::string> ResourceEnumerator::listPostProcessEffects() {
    return listCompositors();
}

} // namespace bbfx
