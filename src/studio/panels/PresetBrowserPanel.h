#pragma once
#include "NodeEditorPanel.h"
#include <imgui.h>
#include <string>
#include <map>
#include <sol/forward.hpp>

namespace bbfx {

/// Browse presets (via Lua Preset:list()), drag-to-graph instantiation.
class PresetBrowserPanel {
public:
    PresetBrowserPanel(NodeEditorPanel* nodeEditor, sol::state& lua);

    void render();

private:
    void renderPresetTree();

    NodeEditorPanel* mNodeEditor;
    sol::state& mLua;
    std::string mSelectedPreset;

    std::map<std::string, std::string> mPresetCategories; // name -> category cache

    // Preset source filter (v3.5 Lot C): 0=All / 1=Builtin / 2=Plugin
    int mPresetSourceFilter = 0;

public:
    // Preset wheel integration (v3.2.5)
    using WheelToggleCb = std::function<void(const std::string& presetName, bool add)>;
    using WheelCheckCb = std::function<bool(const std::string& presetName)>;
    void setWheelCallbacks(WheelToggleCb toggle, WheelCheckCb check) {
        mWheelToggle = std::move(toggle); mWheelCheck = std::move(check);
    }
private:
    WheelToggleCb mWheelToggle;
    WheelCheckCb mWheelCheck;
};

} // namespace bbfx
