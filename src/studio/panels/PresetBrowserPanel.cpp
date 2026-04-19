#include "PresetBrowserPanel.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/PrimitiveNodes.h"
#include "../../plugin/PluginManager.h"
#include "../../plugin/PluginRegistry.h"

#include "../ResourceEnumerator.h"
#include "../TextureThumbnailCache.h"
#include "../ShaderPreviewRenderer.h"

#include <imgui.h>
#include <sol/sol.hpp>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cctype>

namespace bbfx {

PresetBrowserPanel::PresetBrowserPanel(NodeEditorPanel* nodeEditor, sol::state& lua)
    : mNodeEditor(nodeEditor), mLua(lua)
{
    // Initialize quick access slots as empty
    for (auto& slot : mQuickSlots) {
        slot.label  = "+";
        slot.target = "";
    }
}

void PresetBrowserPanel::render() {
    ImGui::Begin("Presets");

    renderPresetTree();
    ImGui::Separator();
    renderAssetBrowser();
    ImGui::Separator();
    renderEffectRack();
    ImGui::Separator();
    renderQuickAccessBar();

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

bool PresetBrowserPanel::matchesSearch(const std::string& name) const {
    if (mAssetSearchBuf[0] == '\0') return true;
    std::string lower = name;
    std::string query = mAssetSearchBuf;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);
    return lower.find(query) != std::string::npos;
}

void PresetBrowserPanel::renderAssetBrowser() {
    ImGui::TextDisabled("Assets");

    // Search bar
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##assetSearch", "Search assets...", mAssetSearchBuf, sizeof(mAssetSearchBuf));

    // Type filter tabs
    if (ImGui::BeginTabBar("##assetTypeFilter")) {
        static const char* tabNames[] = {"All","Meshes","Textures","Particles","Compositors","Shaders","Materials"};
        for (int i = 0; i < 7; ++i) {
            if (ImGui::BeginTabItem(tabNames[i])) {
                mAssetTypeFilter = i;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    bool anyResults = false;

    // Meshes
    if (mAssetTypeFilter == 0 || mAssetTypeFilter == 1) {
        auto meshes = ResourceEnumerator::listMeshes();
        bool hasMatch = false;
        for (auto& m : meshes) { if (matchesSearch(m)) { hasMatch = true; break; } }
        if (hasMatch && ImGui::TreeNode("Meshes")) {
            for (auto& m : meshes) {
                if (!matchesSearch(m)) continue;
                anyResults = true;
                ImGui::Selectable(m.c_str());
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("MESH_NAME", m.c_str(), m.size() + 1);
                    ImGui::Text("Mesh: %s", m.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("3D Mesh: %s", m.c_str());
                }
            }
            ImGui::TreePop();
        }
    }

    // Textures
    if (mAssetTypeFilter == 0 || mAssetTypeFilter == 2) {
        auto textures = ResourceEnumerator::listTextures();
        bool hasMatch = false;
        for (auto& t : textures) { if (matchesSearch(t)) { hasMatch = true; break; } }
        if (hasMatch && ImGui::TreeNode("Textures")) {
            // Toggle grid/list button
            if (ImGui::SmallButton(mTextureGridView ? "List" : "Grid")) {
                mTextureGridView = !mTextureGridView;
            }

            if (mTextureGridView && mThumbCache) {
                // Grid view with thumbnails
                float thumbSz = 64.0f;
                float padding = 4.0f;
                float panelW = ImGui::GetContentRegionAvail().x;
                for (auto& t : textures) {
                    if (!matchesSearch(t)) continue;
                    anyResults = true;

                    ImTextureID thumb = mThumbCache->getThumbnail(t);
                    ImGui::BeginGroup();
                    ImGui::Image(thumb, {thumbSz, thumbSz});
                    // Drag source on the image (SourceAllowNullID needed for Image items)
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                        ImGui::SetDragDropPayload("TEXTURE_NAME", t.c_str(), t.size() + 1);
                        ImGui::Image(thumb, {32, 32});
                        ImGui::SameLine();
                        ImGui::Text("%s", t.c_str());
                        ImGui::EndDragDropSource();
                    }
                    // Tooltip with larger preview
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Image(thumb, {128, 128});
                        ImGui::Text("%s", t.c_str());
                        ImGui::EndTooltip();
                    }
                    // Truncated name under thumbnail
                    std::string shortName = t.size() > 10 ? t.substr(0, 9) + "~" : t;
                    ImGui::TextDisabled("%s", shortName.c_str());
                    ImGui::EndGroup();

                    // Wrap: SameLine if next item fits
                    float nextX = ImGui::GetCursorPosX() + thumbSz + padding;
                    float itemX = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + padding + thumbSz;
                    if (itemX < panelW) ImGui::SameLine(0, padding);
                }
                ImGui::NewLine();
            } else {
                // List view (fallback or no thumb cache)
                for (auto& t : textures) {
                    if (!matchesSearch(t)) continue;
                    anyResults = true;
                    ImGui::Selectable(t.c_str());
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("TEXTURE_NAME", t.c_str(), t.size() + 1);
                        ImGui::Text("Texture: %s", t.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::IsItemHovered() && mThumbCache) {
                        ImGui::BeginTooltip();
                        ImGui::Image(mThumbCache->getThumbnail(t), {128, 128});
                        ImGui::Text("%s", t.c_str());
                        ImGui::EndTooltip();
                    }
                }
            }
            ImGui::TreePop();
        }
    }

    // Particles
    if (mAssetTypeFilter == 0 || mAssetTypeFilter == 3) {
        auto particles = ResourceEnumerator::listParticleTemplates();
        bool hasMatch = false;
        for (auto& p : particles) { if (matchesSearch(p)) { hasMatch = true; break; } }
        if (hasMatch && ImGui::TreeNode("Particles")) {
            for (auto& p : particles) {
                if (!matchesSearch(p)) continue;
                anyResults = true;
                ImGui::Selectable(p.c_str());
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("PARTICLE_NAME", p.c_str(), p.size() + 1);
                    ImGui::Text("Particle: %s", p.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Particle: %s", p.c_str());
                }
            }
            ImGui::TreePop();
        }
    }

    // Compositors
    if (mAssetTypeFilter == 0 || mAssetTypeFilter == 4) {
        auto comps = ResourceEnumerator::listCompositors();
        bool hasMatch = false;
        for (auto& c : comps) { if (matchesSearch(c)) { hasMatch = true; break; } }
        if (hasMatch && ImGui::TreeNode("Compositors")) {
            for (auto& c : comps) {
                if (!matchesSearch(c)) continue;
                anyResults = true;
                ImGui::Selectable(c.c_str());
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("COMPOSITOR_NAME", c.c_str(), c.size() + 1);
                    ImGui::Text("Compositor: %s", c.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Post-process: %s", c.c_str());
                }
            }
            ImGui::TreePop();
        }
    }

    // Shaders (with animated preview thumbnails if ShaderPreviewRenderer available)
    if (mAssetTypeFilter == 0 || mAssetTypeFilter == 5) {
        auto shaders = ResourceEnumerator::listShaders();
        bool hasMatch = false;
        for (auto& s : shaders) { if (matchesSearch(s)) { hasMatch = true; break; } }
        if (hasMatch && ImGui::TreeNode("Shaders")) {
            float contentW = ImGui::GetContentRegionAvail().x;
            int cols = (mPreviewRenderer) ? std::max(1, static_cast<int>(contentW / 76.0f)) : 1;
            int col = 0;
            for (auto& s : shaders) {
                if (!matchesSearch(s)) continue;
                anyResults = true;

                if (mPreviewRenderer && mPreviewRenderer->isInitialized()) {
                    // Grid view with animated preview
                    if (col > 0 && col < cols) ImGui::SameLine();
                    ImGui::BeginGroup();
                    ImTextureID preview = mPreviewRenderer->getShaderPreview(s);
                    ImGui::Image(preview, {64, 64});
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                        ImGui::SetDragDropPayload("SHADER_NAME", s.c_str(), s.size() + 1);
                        ImGui::Image(preview, {32, 32});
                        ImGui::SameLine();
                        ImGui::Text("%s", s.c_str());
                        ImGui::EndDragDropSource();
                    }
                    // Truncate name
                    std::string displayName = s;
                    if (displayName.size() > 10) displayName = displayName.substr(0, 8) + "..";
                    ImGui::TextDisabled("%s", displayName.c_str());
                    ImGui::EndGroup();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shader: %s", s.c_str());
                    col = (col + 1) % cols;
                } else {
                    // Fallback: text list
                    ImGui::Selectable(s.c_str());
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("SHADER_NAME", s.c_str(), s.size() + 1);
                        ImGui::Text("Shader: %s", s.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shader: %s", s.c_str());
                }
            }
            ImGui::TreePop();
        }
    }

    // Materials (with sphere preview thumbnails if ShaderPreviewRenderer available)
    if (mAssetTypeFilter == 0 || mAssetTypeFilter == 6) {
        auto materials = ResourceEnumerator::listMaterials();
        bool hasMatch = false;
        for (auto& m : materials) { if (matchesSearch(m)) { hasMatch = true; break; } }
        if (hasMatch && ImGui::TreeNode("Materials")) {
            float contentW = ImGui::GetContentRegionAvail().x;
            int cols = (mPreviewRenderer) ? std::max(1, static_cast<int>(contentW / 76.0f)) : 1;
            int col = 0;
            for (auto& m : materials) {
                if (!matchesSearch(m)) continue;
                anyResults = true;

                if (mPreviewRenderer && mPreviewRenderer->isInitialized()) {
                    // Grid view with sphere preview
                    if (col > 0 && col < cols) ImGui::SameLine();
                    ImGui::BeginGroup();
                    ImTextureID preview = mPreviewRenderer->getMaterialPreview(m);
                    ImGui::Image(preview, {64, 64});
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                        ImGui::SetDragDropPayload("MATERIAL_NAME", m.c_str(), m.size() + 1);
                        ImGui::Image(preview, {32, 32});
                        ImGui::SameLine();
                        ImGui::Text("%s", m.c_str());
                        ImGui::EndDragDropSource();
                    }
                    std::string displayName = m;
                    if (displayName.size() > 10) displayName = displayName.substr(0, 8) + "..";
                    ImGui::TextDisabled("%s", displayName.c_str());
                    ImGui::EndGroup();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Material: %s", m.c_str());
                    col = (col + 1) % cols;
                } else {
                    // Fallback: text list
                    ImGui::Selectable(m.c_str());
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("MATERIAL_NAME", m.c_str(), m.size() + 1);
                        ImGui::Text("Material: %s", m.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Material: %s", m.c_str());
                }
            }
            ImGui::TreePop();
        }
    }

    if (!anyResults && mAssetSearchBuf[0] != '\0') {
        ImGui::TextDisabled("No results");
    }
}

void PresetBrowserPanel::renderEffectRack() {
    ImGui::TextDisabled("Effect Rack");

    auto* animator = Animator::instance();
    if (!animator) return;

    for (auto& name : animator->getRegisteredNodeNames()) {
        auto* node = animator->getRegisteredNode(name);
        if (!node) continue;

        bool active = (mBypassedNodes.find(name) == mBypassedNodes.end());
        std::string checkLabel = "##rack_" + name;
        if (ImGui::Checkbox(checkLabel.c_str(), &active)) {
            if (!active) {
                mBypassedNodes.insert(name);
                // Zero all output port values
                for (auto& [portName, port] : node->getOutputs()) {
                    port->setValue(0.0f);
                }
            } else {
                mBypassedNodes.erase(name);
            }
            std::cout << "[Rack] Toggle " << name << " → " << active << std::endl;
        }
        ImGui::SameLine();
        ImGui::Text("%s", name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", node->getTypeName().c_str());
    }
}

void PresetBrowserPanel::renderQuickAccessBar() {
    ImGui::TextDisabled("Quick Access");

    float btnSize = 48.0f;
    for (int i = 0; i < 8; ++i) {
        if (i > 0 && i % 4 != 0) ImGui::SameLine();

        std::string label = mQuickSlots[i].label + "##qs" + std::to_string(i);
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, {0.5f, 0.5f});

        bool hasTarget = !mQuickSlots[i].target.empty();
        if (hasTarget) {
            ImGui::PushStyleColor(ImGuiCol_Button, {0.0f, 0.4f, 0.4f, 1.0f});
        }

        if (ImGui::Button(label.c_str(), {btnSize, btnSize})) {
            if (hasTarget) {
                // Try to activate chord via Lua ChordSystem
                bool chordHandled = false;
                try {
                    sol::object obj = mLua["ChordSystem"];
                    if (obj.valid() && obj.get_type() == sol::type::table) {
                        sol::table chordSys = obj.as<sol::table>();
                        sol::function activateFn = chordSys["activate"];
                        if (activateFn.valid()) {
                            activateFn(chordSys, mQuickSlots[i].target);
                            chordHandled = true;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[QuickAccess] Chord error: " << e.what() << std::endl;
                }
                if (!chordHandled) {
                    // Fall back: try to instantiate as preset
                    std::string presetPath = "lua/presets/" + mQuickSlots[i].target + ".lua";
                    auto result = mLua.safe_script_file(presetPath, sol::script_pass_on_error);
                    if (result.valid()) {
                        sol::table preset = result;
                        sol::table nodes = preset.get_or<sol::table>("nodes", sol::nil);
                        if (nodes.valid()) {
                            auto* animator = Animator::instance();
                            if (animator) {
                                for (auto& [k, v] : nodes) {
                                    sol::table nodeDesc = v.as<sol::table>();
                                    std::string nodeName = nodeDesc.get_or<std::string>("name", mQuickSlots[i].target);
                                    sol::function noop = mLua.load("return function(node) end")().get<sol::function>();
                                    auto* node = new LuaAnimationNode(nodeName, noop);
                                    node->addInput("in");
                                    node->addOutput("out");
                                    animator->registerNode(node);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (hasTarget) ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        // Drop target: assign chord/preset to this slot
        if (ImGui::BeginDragDropTarget()) {
            if (auto* payload = ImGui::AcceptDragDropPayload("PRESET_NAME")) {
                mQuickSlots[i].target = static_cast<const char*>(payload->Data);
                mQuickSlots[i].label  = mQuickSlots[i].target.substr(0, 6);
            }
            ImGui::EndDragDropTarget();
        }
    }
}

} // namespace bbfx
