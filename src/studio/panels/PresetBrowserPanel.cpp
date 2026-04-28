#include "PresetBrowserPanel.h"
#include "../../plugin/PluginManager.h"
#include "../../plugin/PluginRegistry.h"

#include <imgui.h>
#include <sol/sol.hpp>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace bbfx {

PresetBrowserPanel::PresetBrowserPanel(NodeEditorPanel* nodeEditor, sol::state& lua)
    : mNodeEditor(nodeEditor), mLua(lua)
{
}

void PresetBrowserPanel::render() {
    ImGui::Begin("Presets");

    renderPresetTree();

    ImGui::End();
}

void PresetBrowserPanel::renderPresetTree() {
    ImGui::TextDisabled("Available Presets");

    // v3.5 Lot C: Source filter (All / Builtin / Plugin)
    static const char* kSources[] = { "All", "Builtin", "Plugin" };
    ImGui::SetNextItemWidth(100.0f);
    ImGui::Combo("Source##presetSource", &mPresetSourceFilter, kSources, 3);

    struct PresetInfo {
        std::string name;     // display name (includes "[from <id>]" for plugin presets)
        std::string key;      // unique id used for selection/drag payload
        std::string category;
        std::string pluginId; // empty for builtins
    };
    std::vector<PresetInfo> presets;

    // Builtin presets: scan lua/presets/
    const std::string presetsDir = "lua/presets";
    std::error_code ec;
    if (std::filesystem::is_directory(presetsDir, ec)) {
        for (auto& entry : std::filesystem::directory_iterator(presetsDir, ec)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".lua") continue;
            std::string name = entry.path().stem().string();
            if (name.empty() || name[0] == '_') continue; // skip test presets

            std::string category = "Other";
            auto cacheIt = mPresetCategories.find(name);
            if (cacheIt != mPresetCategories.end()) {
                category = cacheIt->second;
            } else {
                auto result = mLua.safe_script_file(presetsDir + "/" + name + ".lua", sol::script_pass_on_error);
                if (result.valid()) {
                    sol::table t = result;
                    sol::optional<std::string> cat = t["category"];
                    if (cat) category = *cat;
                }
                mPresetCategories[name] = category;
            }
            presets.push_back({name, name, category, ""});
        }
    }

    // Plugin presets: pulled from PluginRegistry (populated by
    // bbfx.plugin.registerPreset inside each enabled plugin's sandbox).
    for (const auto& pluginId : PluginManager::instance().listPlugins()) {
        const PluginInfo* p = PluginManager::instance().getPlugin(pluginId);
        if (!p || p->state != PluginState::ENABLED) continue;
        const auto& plist = PluginRegistry::instance().presetsOf(pluginId);
        for (const auto& fullId : plist) {
            // Display name: strip "<pluginId>." prefix for readability.
            std::string shortName = fullId;
            if (shortName.rfind(pluginId + ".", 0) == 0) {
                shortName = shortName.substr(pluginId.size() + 1);
            }
            std::string display = shortName + " [from " + pluginId + "]";
            presets.push_back({display, fullId, "Community/" + p->manifest.name, pluginId});
        }
    }

    // Apply source filter
    std::vector<PresetInfo> filtered;
    filtered.reserve(presets.size());
    for (auto& p : presets) {
        if (mPresetSourceFilter == 1 && !p.pluginId.empty()) continue;  // Builtin only
        if (mPresetSourceFilter == 2 &&  p.pluginId.empty()) continue;  // Plugin only
        filtered.push_back(std::move(p));
    }

    if (filtered.empty()) {
        ImGui::TextDisabled("(no presets match current filter)");
        return;
    }

    // Group by category
    std::map<std::string, std::vector<PresetInfo>> byCategory;
    for (auto& p : filtered) byCategory[p.category].push_back(p);
    for (auto& [cat, list] : byCategory) {
        std::sort(list.begin(), list.end(),
                  [](const PresetInfo& a, const PresetInfo& b) { return a.name < b.name; });
    }

    for (auto& [cat, list] : byCategory) {
        char header[128];
        std::snprintf(header, sizeof(header), "%s (%zu)", cat.c_str(), list.size());
        if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& p : list) {
                bool selected = (mSelectedPreset == p.key);
                if (ImGui::Selectable(p.name.c_str(), selected)) {
                    mSelectedPreset = p.key;
                }
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("PRESET_NAME", p.key.c_str(), p.key.size() + 1);
                    ImGui::Text("Instantiate: %s", p.name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    mSelectedPreset = p.key;
                    ImGui::OpenPopup("PresetContextMenu");
                }
                if (mSelectedPreset == p.key && ImGui::BeginPopup("PresetContextMenu")) {
                    bool inWheel = mWheelCheck ? mWheelCheck(p.key) : false;
                    if (!inWheel && ImGui::MenuItem("Add to Wheel")) {
                        if (mWheelToggle) mWheelToggle(p.key, true);
                    }
                    if (inWheel && ImGui::MenuItem("Remove from Wheel")) {
                        if (mWheelToggle) mWheelToggle(p.key, false);
                    }
                    ImGui::EndPopup();
                }
            }
        }
    }
}

} // namespace bbfx
