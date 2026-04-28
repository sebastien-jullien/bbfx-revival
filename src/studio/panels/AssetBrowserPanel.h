#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <set>
#include <functional>

namespace bbfx {

class TextureThumbnailCache;

/// Asset info descriptor for the Asset Browser.
struct AssetInfo {
    std::string name;
    std::string type;        // "Mesh", "Texture", "Material", "Shader", "Particle", "Effect", "Preset", "Template"
    std::string description;
    std::vector<std::string> tags;
    int vertexCount = 0;     // meshes only
    int triangleCount = 0;   // meshes only
    int nodeCount = 0;       // presets only
};

/// Full-featured Asset Browser panel for BBFx Studio.
/// Provides category sidebar, grid/list views, search, tag filtering,
/// favorites, drag & drop, and info tooltips.
class AssetBrowserPanel {
public:
    AssetBrowserPanel();

    void render();

    void setThumbCache(TextureThumbnailCache* cache) { mThumbCache = cache; }

    /// Callback when user drops an asset onto the viewport.
    using DropCallback = std::function<void(const std::string& type, const std::string& name)>;
    void setDropCallback(DropCallback cb) { mDropCallback = std::move(cb); }

private:
    // ── Data ────────────────────────────────────────────────────────────
    void refreshAssets();
    std::vector<AssetInfo> mAllAssets;
    std::vector<const AssetInfo*> mFilteredAssets;
    bool mNeedsRefresh = true;

    // ── Categories ──────────────────────────────────────────────────────
    enum class Category { All, Meshes, Textures, Materials, Shaders, Particles, Effects, Presets, Templates };
    Category mSelectedCategory = Category::All;
    int countByCategory(Category cat) const;

    // ── View mode ───────────────────────────────────────────────────────
    bool mGridView = true; // true=grid, false=list

    // ── Search ──────────────────────────────────────────────────────────
    char mSearchBuf[256] = {};

    // ── Tag filtering ───────────────────────────────────────────────────
    std::set<std::string> mAllTags;
    std::set<std::string> mActiveTags;

    // ── Favorites ───────────────────────────────────────────────────────
    std::set<std::string> mFavorites; // "Type:Name" keys
    void loadFavorites();
    void saveFavorites();
    bool isFavorite(const AssetInfo& a) const;
    void toggleFavorite(const AssetInfo& a);
    std::string favoriteKey(const AssetInfo& a) const;

    // ── Rendering helpers ───────────────────────────────────────────────
    void renderSidebar();
    void renderToolbar();
    void renderGrid();
    void renderList();
    void renderTooltip(const AssetInfo& a);
    void renderContextMenu(const AssetInfo& a);
    void applyFilters();

    // ── Drag & drop ─────────────────────────────────────────────────────
    void beginDragDrop(const AssetInfo& a);

    // ── Dependencies ────────────────────────────────────────────────────
    TextureThumbnailCache* mThumbCache = nullptr;
    DropCallback mDropCallback;

    // ── Favorites persistence path ──────────────────────────────────────
    std::string mFavoritesPath;
};

} // namespace bbfx
