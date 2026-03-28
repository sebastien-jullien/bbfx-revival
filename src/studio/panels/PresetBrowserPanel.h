#pragma once
#include "NodeEditorPanel.h"
#include <string>
#include <vector>
#include <set>

namespace bbfx {

/// Browse presets (via Lua Preset:list()), drag-to-graph instantiation, effect rack, quick access bar.
class PresetBrowserPanel {
public:
    explicit PresetBrowserPanel(NodeEditorPanel* nodeEditor);

    void render();

private:
    void renderPresetTree();
    void renderEffectRack();
    void renderQuickAccessBar();

    NodeEditorPanel* mNodeEditor;
    std::string mSelectedPreset;

    struct QuickSlot {
        std::string label;
        std::string target; // chord name or preset name
    };
    QuickSlot mQuickSlots[8];
    std::set<std::string> mBypassedNodes;
};

} // namespace bbfx
