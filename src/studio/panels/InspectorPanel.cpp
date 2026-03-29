#include "InspectorPanel.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/AnimationPort.h"
#include "../../core/PrimitiveNodes.h"
#include "../commands/CommandManager.h"
#include "../commands/EditCommands.h"

#include <imgui.h>
#include <sol/sol.hpp>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>

namespace bbfx {

InspectorPanel::InspectorPanel(sol::state& lua) : mLua(lua) {}

void InspectorPanel::render() {
    ImGui::Begin("Inspector");

    if (mSelectedNode.empty()) {
        ImGui::TextDisabled("No node selected");
        ImGui::End();
        return;
    }

    auto* animator = Animator::instance();
    if (!animator) { ImGui::End(); return; }

    auto* node = animator->getRegisteredNode(mSelectedNode);
    if (!node) {
        ImGui::TextDisabled("Node not found: %s", mSelectedNode.c_str());
        mSelectedNode.clear();
        ImGui::End();
        return;
    }

    // ── Header ───────────────────────────────────────────────────────────────
    ImGui::TextColored({0.0f, 1.0f, 1.0f, 1.0f}, "%s", node->getTypeName().c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("  %s", mSelectedNode.c_str());
    ImGui::Separator();

    renderFloatPorts();
    ImGui::Separator();
    renderEnumPorts();
    ImGui::Separator();
    renderLuaEditor();
    renderShaderUniforms();
    ImGui::Separator();
    renderRenameDelete();

    ImGui::End();
}

void InspectorPanel::renderFloatPorts() {
    auto* animator = Animator::instance();
    auto* node = animator->getRegisteredNode(mSelectedNode);
    if (!node) return;

    ImGui::TextDisabled("Input Ports");
    for (auto& [portName, port] : node->getInputs()) {
        float val = port->getValue();
        std::string label = "##in_" + portName;
        ImGui::Text("%s", portName.c_str());
        ImGui::SameLine(80.0f);
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::SliderFloat(label.c_str(), &val, -10.0f, 10.0f)) {
            port->setValue(val);
        }
        // Coalescing: save old value when drag starts
        if (ImGui::IsItemActivated()) {
            mCoalescing.active = true;
            mCoalescing.nodeName = mSelectedNode;
            mCoalescing.portName = portName;
            mCoalescing.oldValue = port->getValue();
        }
        // Commit undo command when drag ends
        if (ImGui::IsItemDeactivatedAfterEdit() && mCoalescing.active
            && mCoalescing.nodeName == mSelectedNode && mCoalescing.portName == portName) {
            CommandManager::instance().execute(
                std::make_unique<EditPortValueCommand>(
                    mCoalescing.nodeName, mCoalescing.portName,
                    mCoalescing.oldValue, val));
            mCoalescing.active = false;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        std::string numLabel = "##n_" + portName;
        if (ImGui::InputFloat(numLabel.c_str(), &val, 0.0f, 0.0f, "%.4f")) {
            port->setValue(val);
        }
    }
}

void InspectorPanel::renderEnumPorts() {
    auto* animator = Animator::instance();
    auto* node = animator->getRegisteredNode(mSelectedNode);
    if (!node) return;

    // Helper: case-insensitive substring check
    auto containsCI = [](const std::string& haystack, const char* needle) {
        std::string lower = haystack;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return lower.find(needle) != std::string::npos;
    };

    bool anyEnum = false;
    for (auto& [portName, port] : node->getInputs()) {
        const char* const* items = nullptr;
        int itemCount = 0;

        if (containsCI(portName, "waveform")) {
            static const char* waveformItems[] = {"sin", "tri", "square", "saw"};
            items = waveformItems;
            itemCount = 4;
        } else if (containsCI(portName, "interpolation")) {
            static const char* interpItems[] = {"linear", "spline"};
            items = interpItems;
            itemCount = 2;
        } else if (containsCI(portName, "mode")) {
            static const char* modeItems[] = {"off", "on", "auto"};
            items = modeItems;
            itemCount = 3;
        }

        if (!items) continue;

        if (!anyEnum) {
            ImGui::TextDisabled("Enum Ports");
            anyEnum = true;
        }

        int current = static_cast<int>(port->getValue());
        if (current < 0) current = 0;
        if (current >= itemCount) current = itemCount - 1;

        std::string label = "##enum_" + portName;
        ImGui::Text("%s", portName.c_str());
        ImGui::SameLine(80.0f);
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::Combo(label.c_str(), &current, items, itemCount)) {
            port->setValue(static_cast<float>(current));
        }
    }
}

void InspectorPanel::renderLuaEditor() {
    auto* animator = Animator::instance();
    auto* node = animator->getRegisteredNode(mSelectedNode);
    if (!node) return;

    if (node->getTypeName() != "LuaAnimationNode") return;
    auto* luaNode = dynamic_cast<LuaAnimationNode*>(node);

    // Load source from node into buffer when selecting a different node
    static std::string lastLoadedNode;
    if (luaNode && mSelectedNode != lastLoadedNode) {
        const auto& src = luaNode->getSource();
        std::strncpy(mLuaSourceBuf, src.c_str(), sizeof(mLuaSourceBuf) - 1);
        mLuaSourceBuf[sizeof(mLuaSourceBuf) - 1] = '\0';
        mLuaModified = false;
        mLuaError.clear();
        lastLoadedNode = mSelectedNode;
    }

    ImGui::TextDisabled("Lua Source");
    if (mLuaModified) {
        ImGui::SameLine();
        ImGui::TextColored({1.0f, 0.8f, 0.0f, 1.0f}, "(modified)");
    }

    ImGui::InputTextMultiline("##luasrc", mLuaSourceBuf, sizeof(mLuaSourceBuf),
        {-1.0f, 120.0f});

    if (ImGui::IsItemEdited()) mLuaModified = true;

    if (ImGui::Button("Apply") && mLuaModified) {
        std::string src(mLuaSourceBuf);
        auto loadResult = mLua.load("return function(node) " + src + " end");
        if (loadResult.valid()) {
            sol::protected_function factory = loadResult;
            auto callResult = factory();
            if (callResult.valid()) {
                sol::function updateFn = callResult;
                auto* luaNode = dynamic_cast<LuaAnimationNode*>(node);
                if (luaNode) {
                    luaNode->setUpdateFunction(updateFn);
                    luaNode->setSource(src);
                    mLuaError.clear();
                }
            }
        } else {
            sol::error err = loadResult;
            mLuaError = err.what();
        }
        mLuaModified = false;
    }
    if (!mLuaError.empty()) {
        ImGui::TextColored({1, 0, 0, 1}, "%s", mLuaError.c_str());
    }
}

void InspectorPanel::renderShaderUniforms() {
    auto* animator = Animator::instance();
    auto* node = animator->getRegisteredNode(mSelectedNode);
    if (!node) return;
    if (node->getTypeName() != "ShaderFxNode") return;

    ImGui::TextDisabled("Shader Uniforms");
    for (auto& [portName, port] : node->getInputs()) {
        float val = port->getValue();
        std::string label = "##uni_" + portName;
        ImGui::Text("%s", portName.c_str());
        ImGui::SameLine(80.0f);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderFloat(label.c_str(), &val, -10.0f, 10.0f)) {
            port->setValue(val);
        }
    }
}

void InspectorPanel::renderRenameDelete() {
    static char nameBuf[128] = {};
    if (mSelectedNode.size() < sizeof(nameBuf)) {
        std::strncpy(nameBuf, mSelectedNode.c_str(), sizeof(nameBuf) - 1);
    }

    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::InputText("Name##rename", nameBuf, sizeof(nameBuf),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string newName(nameBuf);
        if (!newName.empty() && newName != mSelectedNode) {
            CommandManager::instance().execute(
                std::make_unique<RenameNodeCommand>(mSelectedNode, newName));
            mSelectedNode = newName;
        }
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.6f, 0.0f, 0.0f, 1.0f});
    if (ImGui::Button("Delete")) {
        ImGui::OpenPopup("ConfirmDelete");
    }
    ImGui::PopStyleColor();

    if (ImGui::BeginPopupModal("ConfirmDelete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete node '%s'?", mSelectedNode.c_str());
        ImGui::Separator();
        if (ImGui::Button("Yes", {80, 0})) {
            auto* animator = Animator::instance();
            if (animator) {
                auto* node = animator->getRegisteredNode(mSelectedNode);
                if (node) animator->removeNode(node);
            }
            mSelectedNode.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {80, 0})) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace bbfx
