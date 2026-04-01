#pragma once
#include "NodeEditorPanel.h"
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
};

} // namespace bbfx
