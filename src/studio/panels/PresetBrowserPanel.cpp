#include "PresetBrowserPanel.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"

#include <imgui.h>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <vector>

namespace bbfx {

PresetBrowserPanel::PresetBrowserPanel(NodeEditorPanel* nodeEditor)
    : mNodeEditor(nodeEditor)
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
    renderEffectRack();
    ImGui::Separator();
    renderQuickAccessBar();

    ImGui::End();
}

void PresetBrowserPanel::renderPresetTree() {
    ImGui::TextDisabled("Available Presets");

    // Scan lua/presets/ directory for .lua files
    std::vector<std::string> presets;
    const std::string presetsDir = "lua/presets";
    std::error_code ec;
    if (std::filesystem::is_directory(presetsDir, ec)) {
        for (auto& entry : std::filesystem::directory_iterator(presetsDir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                presets.push_back(entry.path().stem().string());
            }
        }
        std::sort(presets.begin(), presets.end());
    }

    if (presets.empty()) {
        ImGui::TextDisabled("(no presets found in lua/presets/)");
    }

    for (auto& p : presets) {
        bool selected = (mSelectedPreset == p);
        if (ImGui::Selectable(p.c_str(), selected)) {
            mSelectedPreset = p;
        }

        // Drag source: drag preset name to node editor
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("PRESET_NAME", p.c_str(), p.size() + 1);
            ImGui::Text("Instantiate: %s", p.c_str());
            ImGui::EndDragDropSource();
        }
    }

    // Drop target on node editor handled in NodeEditorPanel (I-236)
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
                std::cout << "[QuickAccess] Trigger: " << mQuickSlots[i].target << std::endl;
                // TODO: trigger chord state or instantiate preset
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
