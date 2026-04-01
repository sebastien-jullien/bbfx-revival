#include "InspectorPanel.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/AnimationPort.h"
#include "../../core/PrimitiveNodes.h"
#include "../commands/CommandManager.h"
#include "../commands/EditCommands.h"
#include "../commands/NodeCommands.h"
#include "../../core/ParamSpec.h"

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

    // If the node has a ParamSpec, render typed widgets instead of generic float sliders
    if (node->getParamSpec() && !node->getParamSpec()->empty()) {
        renderParamSpec();
        ImGui::Separator();
    } else {
        renderFloatPorts();
        ImGui::Separator();
        renderEnumPorts();
        ImGui::Separator();
    }
    renderLuaEditor();
    renderShaderUniforms();
    ImGui::Separator();
    renderRenameDelete();

    ImGui::End();
}

void InspectorPanel::renderParamSpec() {
    auto* animator = Animator::instance();
    auto* node = animator->getRegisteredNode(mSelectedNode);
    if (!node || !node->getParamSpec()) return;

    auto* spec = node->getParamSpec();
    ImGui::TextDisabled("Parameters");

    for (auto& param : const_cast<std::vector<ParamDef>&>(spec->getParams())) {
        const std::string& label = param.displayLabel();
        std::string id = "##ps_" + param.name;

        switch (param.type) {
            case ParamType::FLOAT: {
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine(120.0f);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::SliderFloat(id.c_str(), &param.floatVal, param.minVal, param.maxVal)) {
                    // Sync to DAG port if exists
                    auto& inputs = node->getInputs();
                    auto it = inputs.find(param.name);
                    if (it != inputs.end()) it->second->setValue(param.floatVal);
                }
                break;
            }
            case ParamType::INT: {
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine(120.0f);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SliderInt(id.c_str(), &param.intVal,
                    static_cast<int>(param.minVal), static_cast<int>(param.maxVal));
                break;
            }
            case ParamType::BOOL: {
                ImGui::Checkbox((label + id).c_str(), &param.boolVal);
                break;
            }
            case ParamType::STRING:
            case ParamType::MESH:
            case ParamType::TEXTURE:
            case ParamType::MATERIAL:
            case ParamType::SHADER:
            case ParamType::PARTICLE:
            case ParamType::COMPOSITOR: {
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine(120.0f);
                ImGui::SetNextItemWidth(-1.0f);
                char buf[256];
                std::strncpy(buf, param.stringVal.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                if (ImGui::InputText(id.c_str(), buf, sizeof(buf))) {
                    param.stringVal = buf;
                }
                break;
            }
            case ParamType::ENUM: {
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine(120.0f);
                ImGui::SetNextItemWidth(-1.0f);
                if (!param.choices.empty()) {
                    int current = 0;
                    for (int i = 0; i < static_cast<int>(param.choices.size()); i++) {
                        if (param.choices[i] == param.stringVal) { current = i; break; }
                    }
                    std::string preview = param.choices[current];
                    if (ImGui::BeginCombo(id.c_str(), preview.c_str())) {
                        for (int i = 0; i < static_cast<int>(param.choices.size()); i++) {
                            bool selected = (i == current);
                            if (ImGui::Selectable(param.choices[i].c_str(), selected)) {
                                param.stringVal = param.choices[i];
                                auto& inputs = node->getInputs();
                                auto it = inputs.find(param.name);
                                if (it != inputs.end()) it->second->setValue(static_cast<float>(i));
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                break;
            }
            case ParamType::COLOR: {
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine(120.0f);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::ColorEdit3(id.c_str(), param.colorVal,
                    ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_DisplayRGB);
                // Color harmonies display
                {
                    float h, s, v;
                    ImGui::ColorConvertRGBtoHSV(param.colorVal[0], param.colorVal[1], param.colorVal[2], h, s, v);
                    // Complementary
                    float ch = std::fmod(h + 0.5f, 1.0f);
                    float cr, cg, cb;
                    ImGui::ColorConvertHSVtoRGB(ch, s, v, cr, cg, cb);
                    ImVec4 comp(cr, cg, cb, 1.0f);
                    // Triadic
                    float t1h = std::fmod(h + 0.333f, 1.0f);
                    float t2h = std::fmod(h + 0.667f, 1.0f);
                    float t1r,t1g,t1b, t2r,t2g,t2b;
                    ImGui::ColorConvertHSVtoRGB(t1h, s, v, t1r, t1g, t1b);
                    ImGui::ColorConvertHSVtoRGB(t2h, s, v, t2r, t2g, t2b);
                    ImVec4 tri1(t1r, t1g, t1b, 1.0f);
                    ImVec4 tri2(t2r, t2g, t2b, 1.0f);

                    ImGui::Text("  ");
                    ImGui::SameLine();
                    ImGui::ColorButton("Comp##h", comp, 0, {16,16});
                    if (ImGui::IsItemClicked()) { param.colorVal[0]=cr; param.colorVal[1]=cg; param.colorVal[2]=cb; }
                    ImGui::SameLine(); ImGui::TextDisabled("Comp");
                    ImGui::SameLine();
                    ImGui::ColorButton("Tri1##h", tri1, 0, {16,16});
                    if (ImGui::IsItemClicked()) { param.colorVal[0]=t1r; param.colorVal[1]=t1g; param.colorVal[2]=t1b; }
                    ImGui::SameLine();
                    ImGui::ColorButton("Tri2##h", tri2, 0, {16,16});
                    if (ImGui::IsItemClicked()) { param.colorVal[0]=t2r; param.colorVal[1]=t2g; param.colorVal[2]=t2b; }
                    ImGui::SameLine(); ImGui::TextDisabled("Triadic");
                }
                break;
            }
            case ParamType::VEC3: {
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine(120.0f);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::DragFloat3(id.c_str(), param.vec3Val, 0.1f);
                break;
            }
        }
    }
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
            // Defer deletion to start of next frame — calling CommandManager::execute()
            // during ImGui render causes segfault (heap operations during GL render context)
            bbfx::gPendingDeletes.push_back(mSelectedNode);
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
