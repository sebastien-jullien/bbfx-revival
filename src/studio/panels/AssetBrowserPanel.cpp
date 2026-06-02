#include "AssetBrowserPanel.h"
#include "../ResourceEnumerator.h"
#include "../TextureThumbnailCache.h"
#include "../../core/AssetManifest.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <cstring>
#include <map>

namespace bbfx {

// ── Helpers ──────────────────────────────────────────────────────────────

static bool containsCI(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char a, char b) { return std::tolower(a) == std::tolower(b); });
    return it != haystack.end();
}

// ── Construction ─────────────────────────────────────────────────────────

AssetBrowserPanel::AssetBrowserPanel() {
    mFavoritesPath = "bbfx_favorites.json";
    loadFavorites();
}

// ── Favorites persistence ────────────────────────────────────────────────

std::string AssetBrowserPanel::favoriteKey(const AssetInfo& a) const {
    return a.type + ":" + a.name;
}

bool AssetBrowserPanel::isFavorite(const AssetInfo& a) const {
    return mFavorites.count(favoriteKey(a)) > 0;
}

void AssetBrowserPanel::toggleFavorite(const AssetInfo& a) {
    auto key = favoriteKey(a);
    if (mFavorites.count(key))
        mFavorites.erase(key);
    else
        mFavorites.insert(key);
    saveFavorites();
}

void AssetBrowserPanel::loadFavorites() {
    std::ifstream f(mFavoritesPath);
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) mFavorites.insert(line);
    }
}

void AssetBrowserPanel::saveFavorites() {
    std::ofstream f(mFavoritesPath);
    if (!f.is_open()) return;
    for (auto& key : mFavorites) f << key << "\n";
}

// ── Asset discovery ──────────────────────────────────────────────────────

void AssetBrowserPanel::refreshAssets() {
    mAllAssets.clear();
    mAllTags.clear();

    // Meshes
    for (auto& name : ResourceEnumerator::listMeshes()) {
        AssetInfo a;
        a.name = name;
        a.type = "Mesh";
        a.description = "3D mesh resource";
        a.tags = {"geometry", "mesh"};
        mAllAssets.push_back(std::move(a));
    }

    // Textures
    for (auto& name : ResourceEnumerator::listTextures()) {
        AssetInfo a;
        a.name = name;
        a.type = "Texture";
        a.description = "Texture image";
        a.tags = {"texture", "image"};
        mAllAssets.push_back(std::move(a));
    }

    // Materials
    for (auto& name : ResourceEnumerator::listMaterials()) {
        AssetInfo a;
        a.name = name;
        a.type = "Material";
        a.description = "OGRE material";
        a.tags = {"material"};
        mAllAssets.push_back(std::move(a));
    }

    // Shaders
    for (auto& name : ResourceEnumerator::listShaders()) {
        AssetInfo a;
        a.name = name;
        a.type = "Shader";
        a.description = "GLSL shader";
        a.tags = {"shader", "gpu"};
        mAllAssets.push_back(std::move(a));
    }

    // Particle templates
    for (auto& name : ResourceEnumerator::listParticleTemplates()) {
        AssetInfo a;
        a.name = name;
        a.type = "Particle";
        a.description = "Particle system template";
        a.tags = {"particle", "fx"};
        mAllAssets.push_back(std::move(a));
    }

    // Post-process effects (compositors)
    for (auto& name : ResourceEnumerator::listCompositors()) {
        AssetInfo a;
        a.name = name;
        a.type = "Effect";
        a.description = "Post-process compositor effect";
        a.tags = {"effect", "compositor", "postprocess"};
        mAllAssets.push_back(std::move(a));
    }

    // Presets (scan lua/presets/)
    {
        std::error_code ec;
        const std::string dir = "lua/presets";
        if (std::filesystem::is_directory(dir, ec)) {
            for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".lua") continue;
                std::string name = entry.path().stem().string();
                if (name.empty() || name[0] == '_') continue;
                AssetInfo a;
                a.name = name;
                a.type = "Preset";
                a.description = "Lua preset graph";
                a.tags = {"preset"};
                mAllAssets.push_back(std::move(a));
            }
        }
    }

    // Templates (scan lua/templates/)
    {
        std::error_code ec;
        const std::string dir = "lua/templates";
        if (std::filesystem::is_directory(dir, ec)) {
            for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".lua") continue;
                std::string name = entry.path().stem().string();
                if (name.empty() || name[0] == '_') continue;
                AssetInfo a;
                a.name = name;
                a.type = "Template";
                a.description = "Project template";
                a.tags = {"template"};
                mAllAssets.push_back(std::move(a));
            }
        }
    }

    // Collect all tags
    for (auto& a : mAllAssets) {
        for (auto& t : a.tags) mAllTags.insert(t);
    }

    mNeedsRefresh = false;
    applyFilters();
}

// ── Filtering ────────────────────────────────────────────────────────────

int AssetBrowserPanel::countByCategory(Category cat) const {
    if (cat == Category::All) return (int)mAllAssets.size();
    const char* type = nullptr;
    switch (cat) {
        case Category::Meshes:    type = "Mesh";     break;
        case Category::Textures:  type = "Texture";  break;
        case Category::Materials: type = "Material"; break;
        case Category::Shaders:   type = "Shader";   break;
        case Category::Particles: type = "Particle"; break;
        case Category::Effects:   type = "Effect";   break;
        case Category::Presets:   type = "Preset";   break;
        case Category::Templates: type = "Template"; break;
        default: return 0;
    }
    int count = 0;
    for (auto& a : mAllAssets) {
        if (a.type == type) ++count;
    }
    return count;
}

void AssetBrowserPanel::applyFilters() {
    mFilteredAssets.clear();

    std::string search(mSearchBuf);

    // Separate favorites and non-favorites
    std::vector<const AssetInfo*> favs, normals;

    for (auto& a : mAllAssets) {
        // Category filter
        if (mSelectedCategory != Category::All) {
            const char* type = nullptr;
            switch (mSelectedCategory) {
                case Category::Meshes:    type = "Mesh";     break;
                case Category::Textures:  type = "Texture";  break;
                case Category::Materials: type = "Material"; break;
                case Category::Shaders:   type = "Shader";   break;
                case Category::Particles: type = "Particle"; break;
                case Category::Effects:   type = "Effect";   break;
                case Category::Presets:   type = "Preset";   break;
                case Category::Templates: type = "Template"; break;
                default: break;
            }
            if (type && a.type != type) continue;
        }

        // Search filter (name, description, tags)
        if (!search.empty()) {
            bool match = containsCI(a.name, search) || containsCI(a.description, search);
            if (!match) {
                for (auto& t : a.tags) {
                    if (containsCI(t, search)) { match = true; break; }
                }
            }
            if (!match) continue;
        }

        // Tag filter (AND)
        if (!mActiveTags.empty()) {
            bool allMatch = true;
            for (auto& tag : mActiveTags) {
                bool found = false;
                for (auto& t : a.tags) {
                    if (t == tag) { found = true; break; }
                }
                if (!found) { allMatch = false; break; }
            }
            if (!allMatch) continue;
        }

        if (isFavorite(a))
            favs.push_back(&a);
        else
            normals.push_back(&a);
    }

    // Favorites first, then normal
    mFilteredAssets.reserve(favs.size() + normals.size());
    for (auto* p : favs) mFilteredAssets.push_back(p);
    for (auto* p : normals) mFilteredAssets.push_back(p);
}

// ── Main render ──────────────────────────────────────────────────────────

void AssetBrowserPanel::render() {
    if (mNeedsRefresh) refreshAssets();

    ImGui::SetNextWindowSizeConstraints({400, 300}, {2000, 1500});
    if (!ImGui::Begin("Asset Browser")) {
        ImGui::End();
        return;
    }

    // Layout: sidebar left (200px) + main area right
    float sidebarWidth = 200.0f;

    // Sidebar
    ImGui::BeginChild("##ABSidebar", {sidebarWidth, 0}, ImGuiChildFlags_Borders);
    renderSidebar();
    ImGui::EndChild();

    ImGui::SameLine();

    // Main area
    ImGui::BeginChild("##ABMain", {0, 0});
    renderToolbar();
    ImGui::Separator();
    if (mGridView)
        renderGrid();
    else
        renderList();
    ImGui::EndChild();

    ImGui::End();
}

// ── Sidebar ──────────────────────────────────────────────────────────────

void AssetBrowserPanel::renderSidebar() {
    ImGui::TextUnformatted("Categories");
    ImGui::Separator();

    struct CatEntry { Category cat; const char* label; };
    static const CatEntry categories[] = {
        {Category::All,       "All"},
        {Category::Meshes,    "Meshes"},
        {Category::Textures,  "Textures"},
        {Category::Materials, "Materials"},
        {Category::Shaders,   "Shaders"},
        {Category::Particles, "Particles"},
        {Category::Effects,   "Effects"},
        {Category::Presets,   "Presets"},
        {Category::Templates, "Templates"},
    };

    for (auto& [cat, label] : categories) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s (%d)", label, countByCategory(cat));
        bool selected = (mSelectedCategory == cat);
        if (ImGui::Selectable(buf, selected)) {
            mSelectedCategory = cat;
            applyFilters();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Tags");
    ImGui::Separator();

    for (auto& tag : mAllTags) {
        bool active = mActiveTags.count(tag) > 0;
        if (ImGui::Selectable(tag.c_str(), active)) {
            if (active)
                mActiveTags.erase(tag);
            else
                mActiveTags.insert(tag);
            applyFilters();
        }
    }

    if (!mActiveTags.empty()) {
        ImGui::Spacing();
        if (ImGui::SmallButton("Clear Tags")) {
            mActiveTags.clear();
            applyFilters();
        }
    }

    // v3.5.2 Sprint S7 Lot Z — Heritage Pack section.
    ImGui::Spacing();
    ImGui::Separator();
    renderHeritageSection();
}

// v3.5.2 Sprint S7 Lot Z — Heritage Pack browser inside the AssetBrowser sidebar.
// Lists manifest entries grouped by `category`. Drag-and-drop payload `HERITAGE_TEXTURE`
// carries the logical name; drop targets (e.g. NodeEditor canvas) auto-create a
// MaterialBridgeNode resolving the texture via `bbfx::AssetManifest::resolveAndLoad`.
void AssetBrowserPanel::renderHeritageSection() {
    auto& mf = AssetManifest::instance();
    size_t total = mf.entryCount();

    // Header w/ counter
    char hdr[128];
    if (total == 0) {
        std::snprintf(hdr, sizeof(hdr), "Heritage Pack (empty)");
        mHeritageVisible = false;
    } else {
        std::snprintf(hdr, sizeof(hdr), "Heritage Pack (%zu)", total);
        mHeritageVisible = true;
    }

    if (!ImGui::CollapsingHeader(hdr, ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    if (total == 0) {
        ImGui::TextDisabled("Run tools/asset_pipeline.py to populate.");
        mHeritageRenderedCount = 0;
        return;
    }

    // Filter combo
    static const char* kCats[] = {"all", "organic", "cosmic", "geometric", "mask", "gray_pair", "video"};
    if (ImGui::BeginCombo("##heritage_cat", mHeritageCategoryFilter)) {
        for (auto* c : kCats) {
            bool sel = std::strcmp(c, mHeritageCategoryFilter) == 0;
            if (ImGui::Selectable(c, sel)) {
                std::strncpy(mHeritageCategoryFilter, c, sizeof(mHeritageCategoryFilter) - 1);
                mHeritageCategoryFilter[sizeof(mHeritageCategoryFilter) - 1] = '\0';
            }
        }
        ImGui::EndCombo();
    }

    // Group entries by category (post-filter)
    std::map<std::string, std::vector<const AssetManifest::Entry*>> byCat;
    bool catAll = (std::strcmp(mHeritageCategoryFilter, "all") == 0);
    for (auto& [name, e] : mf.getEntries()) {
        if (!catAll && e.category != mHeritageCategoryFilter) continue;
        std::string k = e.category.empty() ? "uncategorized" : e.category;
        byCat[k].push_back(&e);
    }

    size_t rendered = 0;
    for (auto& [cat, entries] : byCat) {
        char treeLabel[128];
        std::snprintf(treeLabel, sizeof(treeLabel), "%s (%zu)", cat.c_str(), entries.size());
        if (ImGui::TreeNode(treeLabel)) {
            for (auto* e : entries) {
                ImGui::Selectable(e->name.c_str());
                ++rendered;

                // Drag source — payload = logical name.
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("HERITAGE_TEXTURE",
                                              e->name.c_str(),
                                              e->name.size() + 1);
                    ImGui::Text("Heritage: %s", e->name.c_str());
                    ImGui::EndDragDropSource();
                }

                // Hover tooltip with metadata (and thumbnail if cache + texture loaded).
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", e->name.c_str());
                    if (!e->license.empty()) {
                        ImGui::TextDisabled("license: %s", e->license.c_str());
                    }
                    if (!e->author.empty()) {
                        ImGui::TextDisabled("author : %s", e->author.c_str());
                    }
                    if (e->size_bytes > 0) {
                        ImGui::TextDisabled("size   : %.1f KB",
                                            (double)e->size_bytes / 1024.0);
                    }
                    if (mThumbCache && !e->filename.empty()) {
                        // Best-effort thumbnail — skip silently if not yet loaded.
                        std::string fn = mf.resolveAndLoad(e->name);
                        if (!fn.empty()) {
                            ImTextureID tid = mThumbCache->getThumbnail(fn);
                            if (tid) {
                                ImGui::Image(tid, ImVec2(96, 96));
                            }
                        }
                    }
                    ImGui::EndTooltip();
                }
            }
            ImGui::TreePop();
        }
    }
    mHeritageRenderedCount = rendered;
}

// ── Toolbar ──────────────────────────────────────────────────────────────

void AssetBrowserPanel::renderToolbar() {
    // Search bar
    ImGui::SetNextItemWidth(-120.0f);
    if (ImGui::InputTextWithHint("##ABSearch", "Search assets...", mSearchBuf, sizeof(mSearchBuf))) {
        applyFilters();
    }

    ImGui::SameLine();

    // Grid/List toggle
    if (ImGui::Button(mGridView ? "List" : "Grid")) {
        mGridView = !mGridView;
    }

    ImGui::SameLine();

    // Refresh
    if (ImGui::Button("Refresh")) {
        ResourceEnumerator::invalidateCache();
        mNeedsRefresh = true;
    }
}

// ── Grid view ────────────────────────────────────────────────────────────

void AssetBrowserPanel::renderGrid() {
    float thumbSize = 64.0f;
    float cellWidth = thumbSize + 16.0f;
    float availWidth = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, (int)(availWidth / cellWidth));

    // Favorites separator
    bool hasFavs = false;
    for (auto* a : mFilteredAssets) {
        if (isFavorite(*a)) { hasFavs = true; break; }
    }

    int idx = 0;
    bool favSeparatorDrawn = false;

    ImGui::BeginChild("##ABGridScroll", {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    for (auto* a : mFilteredAssets) {
        bool fav = isFavorite(*a);
        if (hasFavs && !fav && !favSeparatorDrawn) {
            // End the favorites row
            if (idx % cols != 0) ImGui::NewLine();
            ImGui::Separator();
            ImGui::TextDisabled("All Assets");
            ImGui::Separator();
            favSeparatorDrawn = true;
            idx = 0;
        }

        if (idx > 0 && idx % cols != 0) ImGui::SameLine();

        ImGui::BeginGroup();
        {
            // Thumbnail placeholder (colored rect based on type)
            ImVec4 col = {0.3f, 0.3f, 0.3f, 1.0f};
            if (a->type == "Mesh")     col = {0.2f, 0.5f, 0.8f, 1.0f};
            if (a->type == "Texture")  col = {0.8f, 0.6f, 0.2f, 1.0f};
            if (a->type == "Material") col = {0.6f, 0.3f, 0.7f, 1.0f};
            if (a->type == "Shader")   col = {0.2f, 0.8f, 0.4f, 1.0f};
            if (a->type == "Particle") col = {0.9f, 0.4f, 0.3f, 1.0f};
            if (a->type == "Effect")   col = {0.5f, 0.7f, 0.9f, 1.0f};
            if (a->type == "Preset")   col = {0.9f, 0.8f, 0.2f, 1.0f};
            if (a->type == "Template") col = {0.4f, 0.8f, 0.8f, 1.0f};

            ImVec2 pos = ImGui::GetCursorScreenPos();

            // For textures, try to show actual thumbnail
            if (a->type == "Texture" && mThumbCache) {
                ImTextureID tid = mThumbCache->getThumbnail(a->name);
                if (tid) {
                    ImGui::Image(tid, {thumbSize, thumbSize});
                } else {
                    ImGui::GetWindowDrawList()->AddRectFilled(pos, {pos.x + thumbSize, pos.y + thumbSize},
                        ImGui::ColorConvertFloat4ToU32(col), 4.0f);
                    ImGui::Dummy({thumbSize, thumbSize});
                }
            } else {
                ImGui::GetWindowDrawList()->AddRectFilled(pos, {pos.x + thumbSize, pos.y + thumbSize},
                    ImGui::ColorConvertFloat4ToU32(col), 4.0f);
                // Type label centered in the rect
                const char* typeShort = a->type.c_str();
                ImVec2 textSize = ImGui::CalcTextSize(typeShort);
                ImGui::GetWindowDrawList()->AddText(
                    {pos.x + (thumbSize - textSize.x) * 0.5f, pos.y + (thumbSize - textSize.y) * 0.5f},
                    IM_COL32(255, 255, 255, 200), typeShort);
                ImGui::Dummy({thumbSize, thumbSize});
            }

            // Invisible button for interaction
            ImGui::SetCursorScreenPos(pos);
            char btnId[256];
            std::snprintf(btnId, sizeof(btnId), "##ab_%s_%s", a->type.c_str(), a->name.c_str());
            ImGui::InvisibleButton(btnId, {thumbSize, thumbSize});

            // Double-click → create node
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (mDropCallback) mDropCallback(a->type, a->name);
            }

            // Drag source
            beginDragDrop(*a);

            // Tooltip
            if (ImGui::IsItemHovered()) renderTooltip(*a);

            // Context menu
            if (ImGui::BeginPopupContextItem()) {
                renderContextMenu(*a);
                ImGui::EndPopup();
            }

            // Favorite star
            if (fav) {
                ImGui::GetWindowDrawList()->AddText(
                    {pos.x + thumbSize - 14, pos.y + 2}, IM_COL32(255, 215, 0, 255), "*");
            }

            // Name (truncated)
            std::string displayName = a->name;
            if (displayName.length() > 12) displayName = displayName.substr(0, 10) + "..";
            ImGui::TextUnformatted(displayName.c_str());
        }
        ImGui::EndGroup();

        ++idx;
    }

    ImGui::EndChild();
}

// ── List view ────────────────────────────────────────────────────────────

void AssetBrowserPanel::renderList() {
    // Favorites separator
    bool hasFavs = false;
    for (auto* a : mFilteredAssets) {
        if (isFavorite(*a)) { hasFavs = true; break; }
    }

    bool favSeparatorDrawn = false;

    ImGui::BeginChild("##ABListScroll", {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    for (auto* a : mFilteredAssets) {
        bool fav = isFavorite(*a);
        if (hasFavs && !fav && !favSeparatorDrawn) {
            ImGui::Separator();
            ImGui::TextDisabled("All Assets");
            ImGui::Separator();
            favSeparatorDrawn = true;
        }

        // Row: [type] name - description
        char rowText[512];
        std::snprintf(rowText, sizeof(rowText), "[%s] %s — %s%s",
                      a->type.c_str(), a->name.c_str(), a->description.c_str(),
                      fav ? " *" : "");

        ImGui::Selectable(rowText, false);

        // Double-click → create node
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (mDropCallback) mDropCallback(a->type, a->name);
        }

        // Drag source
        beginDragDrop(*a);

        // Tooltip
        if (ImGui::IsItemHovered()) renderTooltip(*a);

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            renderContextMenu(*a);
            ImGui::EndPopup();
        }
    }

    ImGui::EndChild();
}

// ── Tooltip ──────────────────────────────────────────────────────────────

void AssetBrowserPanel::renderTooltip(const AssetInfo& a) {
    ImGui::BeginTooltip();
    ImGui::Text("Name: %s", a.name.c_str());
    ImGui::Text("Type: %s", a.type.c_str());
    ImGui::Text("Description: %s", a.description.c_str());

    if (a.vertexCount > 0)
        ImGui::Text("Vertices: %d | Triangles: %d", a.vertexCount, a.triangleCount);
    if (a.nodeCount > 0)
        ImGui::Text("Nodes in graph: %d", a.nodeCount);

    if (!a.tags.empty()) {
        ImGui::Text("Tags:");
        ImGui::SameLine();
        for (size_t i = 0; i < a.tags.size(); ++i) {
            if (i > 0) ImGui::SameLine();
            ImGui::TextColored({0.6f, 0.8f, 1.0f, 1.0f}, "%s", a.tags[i].c_str());
        }
    }
    ImGui::EndTooltip();
}

// ── Context menu ─────────────────────────────────────────────────────────

void AssetBrowserPanel::renderContextMenu(const AssetInfo& a) {
    if (isFavorite(a)) {
        if (ImGui::MenuItem("Remove from Favorites")) {
            const_cast<AssetBrowserPanel*>(this)->toggleFavorite(a);
        }
    } else {
        if (ImGui::MenuItem("Add to Favorites")) {
            const_cast<AssetBrowserPanel*>(this)->toggleFavorite(a);
        }
    }
}

// ── Drag & drop ──────────────────────────────────────────────────────────

void AssetBrowserPanel::beginDragDrop(const AssetInfo& a) {
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        // Use the same payload type names as PresetBrowserPanel / InspectorPanel
        // so ViewportPanel and NodeEditorPanel accept them.
        const char* payloadType = "ASSET_GENERIC";
        if (a.type == "Mesh")     payloadType = "MESH_NAME";
        if (a.type == "Texture")  payloadType = "TEXTURE_NAME";
        if (a.type == "Shader")   payloadType = "SHADER_NAME";
        if (a.type == "Particle") payloadType = "PARTICLE_NAME";
        if (a.type == "Material") payloadType = "MATERIAL_NAME";
        if (a.type == "Preset")   payloadType = "PRESET_NAME";
        if (a.type == "Effect")   payloadType = "COMPOSITOR_NAME";
        if (a.type == "Template") payloadType = "TEMPLATE_NAME";
        ImGui::SetDragDropPayload(payloadType, a.name.c_str(), a.name.size() + 1);
        ImGui::Text("%s: %s", a.type.c_str(), a.name.c_str());
        ImGui::EndDragDropSource();
    }
}

} // namespace bbfx
