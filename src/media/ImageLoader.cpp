#include "ImageLoader.h"

#include <filesystem>
#include <iostream>
#include <set>

#include <OgreResourceGroupManager.h>
#include <OgreTexture.h>
#include <OgreTextureManager.h>

namespace bbfx {

namespace {

constexpr const char* kLoaderGroup = "BBFx_ImageLoader";

std::set<std::string>& registeredLocations() {
    static std::set<std::string> s;
    return s;
}

void ensureGroup() {
    auto& rgm = Ogre::ResourceGroupManager::getSingleton();
    if (!rgm.resourceGroupExists(kLoaderGroup)) {
        rgm.createResourceGroup(kLoaderGroup, /*inGlobalPool=*/false);
    }
}

void ensureLocation(const std::string& dir) {
    auto& rgm = Ogre::ResourceGroupManager::getSingleton();
    if (registeredLocations().count(dir)) return;
    try {
        rgm.addResourceLocation(dir, "FileSystem", kLoaderGroup);
        rgm.initialiseResourceGroup(kLoaderGroup);
        registeredLocations().insert(dir);
    } catch (const std::exception& e) {
        std::cerr << "[ImageLoader] addResourceLocation(" << dir << ") failed: "
                   << e.what() << std::endl;
    }
}

} // anonymous

std::string ImageLoader::load(const std::string& source) {
    if (source.empty()) return {};

    auto& tm = Ogre::TextureManager::getSingleton();

    std::filesystem::path p(source);
    std::string name;
    std::string group = Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME;

    if (p.is_absolute() && std::filesystem::exists(p)) {
        ensureGroup();
        ensureLocation(p.parent_path().string());
        name = p.filename().string();
        group = kLoaderGroup;
    } else {
        // Assume already-resolved OGRE resource name.
        name = source;
    }

    try {
        auto tex = tm.getByName(name);
        if (!tex) tex = tm.load(name, group);
        if (tex) return tex->getName();
    } catch (const std::exception& e) {
        std::cerr << "[ImageLoader] load(" << source << ") failed: " << e.what() << std::endl;
    }
    return {};
}

void ImageLoader::release(const std::string& textureName) {
    if (textureName.empty()) return;
    auto& tm = Ogre::TextureManager::getSingleton();
    try {
        auto tex = tm.getByName(textureName);
        if (tex) tm.remove(tex);
    } catch (...) {}
}

} // namespace bbfx
