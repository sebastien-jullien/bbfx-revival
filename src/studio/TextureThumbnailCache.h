#pragma once
#include <imgui.h>
#include <string>
#include <map>

namespace bbfx {

/// Cache of OGRE texture thumbnails as ImGui-compatible GL texture IDs.
/// Lazy-loads on first access, caches result. Returns placeholder on failure.
/// NOTE: Not thread-safe. All access must be from the main (UI/render) thread.
class TextureThumbnailCache {
public:
    TextureThumbnailCache();
    ~TextureThumbnailCache();

    /// Returns an ImTextureID (GL texture) for the named OGRE texture at 64x64.
    /// Lazy-loads and caches. Returns placeholder if texture cannot be loaded.
    ImTextureID getThumbnail(const std::string& textureName);

    /// Clear all cached thumbnails (call when resources are reloaded).
    void invalidate();

private:
    ImTextureID createThumbnailFromOgre(const std::string& textureName);
    void createPlaceholder();

    std::map<std::string, ImTextureID> mCache;
    ImTextureID mPlaceholder = 0;
    static constexpr int kThumbSize = 64;
};

} // namespace bbfx
