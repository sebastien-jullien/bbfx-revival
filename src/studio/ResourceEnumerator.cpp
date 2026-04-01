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

void ResourceEnumerator::invalidateCache() { sCacheDirty = true; }

static std::vector<std::string> scanResourceGroup(const std::string& pattern) {
    std::vector<std::string> result;
    auto& rgm = Ogre::ResourceGroupManager::getSingleton();
    auto files = rgm.findResourceNames("General", pattern);
    for (auto& f : *files) result.push_back(f);
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> ResourceEnumerator::listMeshes() {
    if (sCacheDirty || sMeshes.empty()) {
        sMeshes = scanResourceGroup("*.mesh");
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

std::vector<std::string> ResourceEnumerator::listShaders() {
    if (sCacheDirty || sShaders.empty()) {
        sShaders.clear();
        for (auto& ext : {"*.glsl", "*.frag", "*.vert"}) {
            auto files = scanResourceGroup(ext);
            sShaders.insert(sShaders.end(), files.begin(), files.end());
        }
        std::sort(sShaders.begin(), sShaders.end());
        sShaders.erase(std::unique(sShaders.begin(), sShaders.end()), sShaders.end());
    }
    sCacheDirty = false;
    return sShaders;
}

} // namespace bbfx
