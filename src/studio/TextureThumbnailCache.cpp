#include "TextureThumbnailCache.h"
#include <OgreTextureManager.h>
#include <OgreTexture.h>
#include <OgreResourceGroupManager.h>
#ifdef _WIN32
#  include <windows.h>
#  include <GL/gl.h>
#else
#  include <GL/gl.h>
#endif
#include <iostream>
#include <cstring>

namespace bbfx {

TextureThumbnailCache::TextureThumbnailCache() {
    createPlaceholder();
}

TextureThumbnailCache::~TextureThumbnailCache() {
    invalidate();
    if (mPlaceholder) {
        GLuint id = static_cast<GLuint>(mPlaceholder);
        glDeleteTextures(1, &id);
        mPlaceholder = 0;
    }
}

void TextureThumbnailCache::createPlaceholder() {
    // Create a simple grey checkerboard placeholder
    const int sz = kThumbSize;
    std::vector<unsigned char> pixels(sz * sz * 4);
    for (int y = 0; y < sz; ++y) {
        for (int x = 0; x < sz; ++x) {
            int idx = (y * sz + x) * 4;
            unsigned char c = ((x / 8 + y / 8) % 2 == 0) ? 80 : 60;
            pixels[idx] = c; pixels[idx+1] = c; pixels[idx+2] = c; pixels[idx+3] = 255;
        }
    }
    // Draw a "?" in the center (simplified: just a small lighter block)
    for (int y = 24; y < 40; ++y) {
        for (int x = 24; x < 40; ++x) {
            int idx = (y * sz + x) * 4;
            pixels[idx] = 140; pixels[idx+1] = 140; pixels[idx+2] = 140; pixels[idx+3] = 255;
        }
    }
    GLuint glId;
    glGenTextures(1, &glId);
    glBindTexture(GL_TEXTURE_2D, glId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz, sz, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    mPlaceholder = static_cast<ImTextureID>(glId);
}

ImTextureID TextureThumbnailCache::createThumbnailFromOgre(const std::string& textureName) {
    try {
        // Use OGRE TextureManager to get/load the texture
        auto& texMgr = Ogre::TextureManager::getSingleton();
        Ogre::TexturePtr tex;

        // Try to get existing texture first
        tex = texMgr.getByName(textureName, Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME);
        if (!tex) {
            // Try to load it
            tex = texMgr.load(textureName, Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME,
                              Ogre::TEX_TYPE_2D, 0);
        }
        if (!tex || !tex->isLoaded()) {
            tex->load();
        }
        if (!tex || !tex->isLoaded()) return mPlaceholder;

        // Get the GL texture ID from OGRE
        unsigned int glId = 0;
        tex->getCustomAttribute("GLID", &glId);
        if (glId == 0) return mPlaceholder;

        // Return the OGRE GL texture ID directly — ImGui::Image will scale it to display size
        return static_cast<ImTextureID>(glId);
    } catch (const std::exception& e) {
        std::cerr << "[ThumbCache] Failed to load texture '" << textureName << "': " << e.what() << std::endl;
        return mPlaceholder;
    } catch (...) {
        return mPlaceholder;
    }
}

ImTextureID TextureThumbnailCache::getThumbnail(const std::string& textureName) {
    auto it = mCache.find(textureName);
    if (it != mCache.end()) return it->second;

    ImTextureID thumb = createThumbnailFromOgre(textureName);
    mCache[textureName] = thumb;
    return thumb;
}

void TextureThumbnailCache::invalidate() {
    // Note: we don't delete the GL textures here because they belong to OGRE
    // (we reuse OGRE's GL IDs, not copies). Only the placeholder is ours.
    mCache.clear();
}

} // namespace bbfx
