#include "PresetBrowserPanel.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/PrimitiveNodes.h"

#include "../ResourceEnumerator.h"

#include <imgui.h>
#include <sol/sol.hpp>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <vector>

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

    // Scan lua/presets/ directory for .lua files and read their category
    struct PresetInfo { std::string name; std::string category; };
    std::vector<PresetInfo> presets;
    const std::string presetsDir = "lua/presets";
    std::error_code ec;
    if (std::filesystem::is_directory(presetsDir, ec)) {
        for (auto& entry : std::filesystem::directory_iterator(presetsDir, ec)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".lua") continue;
            std::string name = entry.path().stem().string();
            if (name.empty() || name[0] == '_') continue; // skip test presets

            // Read category from the preset file (cached after first scan)
            std::string category = "Other";
            auto cacheIt = mPresetCategories.find(name);
            if (cacheIt != mPresetCategories.end()) {
                category = cacheIt->second;
            } else {
                // Quick parse: look for category = "..." in the first few lines
                auto result = mLua.safe_script_file(presetsDir + "/" + name + ".lua", sol::script_pass_on_error);
                if (result.valid()) {
                    sol::table t = result;
                    sol::optional<std::string> cat = t["category"];
                    if (cat) category = *cat;
                }
                mPresetCategories[name] = category;
            }
            presets.push_back({name, category});
        }
    }

    if (presets.empty()) {
        ImGui::TextDisabled("(no presets found in lua/presets/)");
        return;
    }

    // Group by category
    std::map<std::string, std::vector<std::string>> byCategory;
    for (auto& p : presets) {
        byCategory[p.category].push_back(p.name);
    }
    for (auto& [cat, names] : byCategory) {
        std::sort(names.begin(), names.end());
    }

    // Render as collapsible accordion sections
    for (auto& [cat, names] : byCategory) {
        // CollapsingHeader with preset count
        char header[128];
        std::snprintf(header, sizeof(header), "%s (%zu)", cat.c_str(), names.size());
        if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& name : names) {
                bool selected = (mSelectedPreset == name);
                if (ImGui::Selectable(name.c_str(), selected)) {
                    mSelectedPreset = name;
                }
                // Drag source: drag preset name to node editor
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("PRESET_NAME", name.c_str(), name.size() + 1);
                    ImGui::Text("Instantiate: %s", name.c_str());
                    ImGui::EndDragDropSource();
                }
            }
        }
    }
}

void PresetBrowserPanel::renderAssetBrowser() {
    ImGui::TextDisabled("Assets");

    // Meshes
    if (ImGui::TreeNode("Meshes")) {
        auto meshes = ResourceEnumerator::listMeshes();
        for (auto& m : meshes) {
            ImGui::Selectable(m.c_str());
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("MESH_NAME", m.c_str(), m.size() + 1);
                ImGui::Text("Mesh: %s", m.c_str());
                ImGui::EndDragDropSource();
            }
        }
        ImGui::TreePop();
    }

    // Textures
    if (ImGui::TreeNode("Textures")) {
        auto textures = ResourceEnumerator::listTextures();
        for (auto& t : textures) {
            ImGui::Selectable(t.c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Texture: %s", t.c_str());
            }
        }
        ImGui::TreePop();
    }

    // Particles
    if (ImGui::TreeNode("Particles")) {
        auto particles = ResourceEnumerator::listParticleTemplates();
        for (auto& p : particles) {
            ImGui::Selectable(p.c_str());
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("PARTICLE_NAME", p.c_str(), p.size() + 1);
                ImGui::Text("Particle: %s", p.c_str());
                ImGui::EndDragDropSource();
            }
        }
        ImGui::TreePop();
    }

    // Compositors
    if (ImGui::TreeNode("Compositors")) {
        auto comps = ResourceEnumerator::listCompositors();
        for (auto& c : comps) {
            ImGui::Selectable(c.c_str());
        }
        ImGui::TreePop();
    }

    // Shaders
    if (ImGui::TreeNode("Shaders")) {
        auto shaders = ResourceEnumerator::listShaders();
        for (auto& s : shaders) {
            ImGui::Selectable(s.c_str());
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("SHADER_NAME", s.c_str(), s.size() + 1);
                ImGui::Text("Shader: %s", s.c_str());
                ImGui::EndDragDropSource();
            }
        }
        ImGui::TreePop();
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
