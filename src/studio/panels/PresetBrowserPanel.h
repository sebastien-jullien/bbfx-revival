#pragma once
#include "NodeEditorPanel.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <sol/forward.hpp>

namespace bbfx {

/// Browse presets (via Lua Preset:list()), drag-to-graph instantiation, effect rack, quick access bar.
class PresetBrowserPanel {
public:
    PresetBrowserPanel(NodeEditorPanel* nodeEditor, sol::state& lua);

    void render();

private:
    void renderPresetTree();
    void renderAssetBrowser();
    void renderEffectRack();
    void renderQuickAccessBar();

    NodeEditorPanel* mNodeEditor;
    sol::state& mLua;
    std::string mSelectedPreset;

    struct QuickSlot {
        std::string label;
        std::string target; // chord name or preset name
    };
    QuickSlot mQuickSlots[8];
    std::set<std::string> mBypassedNodes;
    std::map<std::string, std::string> mPresetCategories; // name -> category cache

    // Asset browser search & filter (v3.2.4)
    char mAssetSearchBuf[128] = {};
    int mAssetTypeFilter = 0; // 0=All, 1=Meshes, 2=Textures, 3=Particles, 4=Compositors, 5=Shaders, 6=Materials
    bool mTextureGridView = true;

    class TextureThumbnailCache* mThumbCache = nullptr; // owned by StudioApp

    bool matchesSearch(const std::string& name) const;
public:
    void setThumbCache(class TextureThumbnailCache* cache) { mThumbCache = cache; }
};

} // namespace bbfx
