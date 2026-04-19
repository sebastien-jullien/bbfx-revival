#include "InspectorPanel.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/AnimationPort.h"
#include "../../core/PrimitiveNodes.h"
#include "../commands/CommandManager.h"
#include "../commands/EditCommands.h"
#include "../commands/NodeCommands.h"
#include "../commands/LinkCommands.h"
#include "../../core/ParamSpec.h"
#include "../ResourceEnumerator.h"
#include "../TextureThumbnailCache.h"
#include "../nodes/SceneObjectNode.h"
#include "../../midi/MidiLearnManager.h"
#include "../../plugin/InspectorWidgetRegistry.h"

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

    // ── Multi-selection header ───────────────────────────────────────────────
    if (mSelectedNodes.size() > 1) {
        ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, "%d nodes selected",
                           static_cast<int>(mSelectedNodes.size()));
        ImGui::Separator();
        for (auto& name : mSelectedNodes) {
            ImGui::BulletText("%s", name.c_str());
        }
        ImGui::Separator();

        // Check if all selected nodes are the same type — if so, show common float params
        auto* animator = Animator::instance();
        if (animator) {
            std::string commonType;
            bool sameType = true;
            for (auto& name : mSelectedNodes) {
                auto* n = animator->getRegisteredNode(name);
                if (!n) { sameType = false; break; }
                if (commonType.empty()) commonType = n->getTypeName();
                else if (n->getTypeName() != commonType) { sameType = false; break; }
            }

            if (sameType && !commonType.empty()) {
                ImGui::TextDisabled("Common type: %s", commonType.c_str());
                ImGui::Separator();

                // Show float ports of the primary node as editable — changes apply to all
                auto* primaryNode = animator->getRegisteredNode(mSelectedNode);
                if (primaryNode) {
                    for (auto& [pname, port] : primaryNode->getInputs()) {
                        if (pname == "entity" || pname == "dt" || pname == "beat" || pname == "beatFrac")
                            continue;
                        float val = port->getValue();
                        std::string label = pname + "##batch";
                        if (ImGui::SliderFloat(label.c_str(), &val, 0.0f, 1.0f)) {
                            // Apply to ALL selected nodes
                            auto compound = std::make_unique<CompoundCommand>("Batch set " + pname);
                            for (auto& sn : mSelectedNodes) {
                                auto* n = animator->getRegisteredNode(sn);
                                if (n) {
                                    auto& inputs = n->getInputs();
                                    auto it = inputs.find(pname);
                                    if (it != inputs.end()) {
                                        float oldVal = it->second->getValue();
                                        compound->add(std::make_unique<EditPortValueCommand>(
                                            sn, pname, oldVal, val));
                                    }
                                }
                            }
                            CommandManager::instance().execute(std::move(compound));
                        }
                    }
                }
            }
        }

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

    // ── FX Stack: Applied Effects (for SceneObjectNode) ─────────────────────
    if (node->getTypeName() == "SceneObjectNode") {
        ImGui::Separator();
        ImGui::TextColored({0.5f, 1.0f, 0.5f, 1.0f}, "Applied Effects");

        // Find all FX nodes linked to this SceneObjectNode's entity port
        auto& outputs = node->getOutputs();
        auto entityIt = outputs.find("entity");
        std::vector<std::string> fxNodes;
        if (entityIt != outputs.end() && animator) {
            for (auto& n2Name : animator->getRegisteredNodeNames()) {
                auto* other = animator->getRegisteredNode(n2Name);
                if (!other || other == node) continue;
                if (other->getParamSpec()) {
                    auto* te = other->getParamSpec()->getParam("target_entity");
                    if (te && te->stringVal == mSelectedNode)
                        fxNodes.push_back(n2Name);
                }
            }
        }
        // Sync with persisted order (adds new FX at end, removes dead ones)
        syncFxOrder(mSelectedNode, fxNodes);

        // Quick-apply FX button "+"
        ImGui::SameLine();
        if (ImGui::SmallButton("+##addFx")) {
            ImGui::OpenPopup("QuickApplyFX");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add effect to this object");
        if (ImGui::BeginPopup("QuickApplyFX")) {
            static int qfxCounter = 0;
            auto quickApply = [&](const std::string& typeName) {
                std::string fxName = typeName + "_" + std::to_string(++qfxCounter);
                auto compound = std::make_unique<CompoundCommand>("Quick apply " + typeName);
                compound->add(std::make_unique<CreateNodeCommand>(typeName, fxName, mLua));
                compound->add(std::make_unique<CreateLinkCommand>(mSelectedNode, "entity", fxName, "entity"));
                CommandManager::instance().execute(std::move(compound));
            };
            if (ImGui::MenuItem("PerlinFxNode")) quickApply("PerlinFxNode");
            if (ImGui::MenuItem("WaveVertexShader")) quickApply("WaveVertexShader");
            if (ImGui::MenuItem("TextureNode")) quickApply("TextureNode");
            if (ImGui::MenuItem("MaterialNode")) quickApply("MaterialNode");
            ImGui::EndPopup();
        }

        if (fxNodes.empty()) {
            ImGui::TextDisabled("No effects applied");
        } else {
            // Drag-reorder support
            static int dragSourceIdx = -1;
            static int dragTargetIdx = -1;

            for (int fi = 0; fi < static_cast<int>(fxNodes.size()); ++fi) {
                auto& fxName = fxNodes[fi];
                auto* fxNode = animator->getRegisteredNode(fxName);
                if (!fxNode) continue;
                ImGui::PushID(fxName.c_str());

                // Drag handle
                ImGui::TextDisabled("::");
                if (ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
                    int delta = ImGui::GetMouseDragDelta(0).y < 0 ? -1 : 1;
                    int nextIdx = fi + delta;
                    if (nextIdx >= 0 && nextIdx < static_cast<int>(fxNodes.size())) {
                        dragSourceIdx = fi;
                        dragTargetIdx = nextIdx;
                    }
                    ImGui::ResetMouseDragDelta();
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Drag to reorder");
                ImGui::SameLine();

                // Enable/disable toggle
                bool en = fxNode->isEnabled();
                if (ImGui::Checkbox("##en", &en)) {
                    CommandManager::instance().execute(
                        std::make_unique<SetEnabledCommand>(fxName, !en, en));
                }
                ImGui::SameLine();

                // Type + name
                ImGui::TextColored({0.7f, 0.7f, 1.0f, 1.0f}, "%s", fxNode->getTypeName().c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("%s", fxName.c_str());

                // Unlink button
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    auto& fxInputs = fxNode->getInputs();
                    auto fxEntityIt = fxInputs.find("entity");
                    if (fxEntityIt != fxInputs.end() && entityIt != outputs.end()) {
                        CommandManager::instance().execute(
                            std::make_unique<DeleteLinkCommand>(
                                mSelectedNode, "entity", fxName, "entity"));
                    }
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Unlink FX");

                ImGui::PopID();
            }

            // Apply drag-reorder swap (persisted in mFxStackOrder)
            if (dragSourceIdx >= 0 && dragTargetIdx >= 0 &&
                dragSourceIdx < static_cast<int>(fxNodes.size()) &&
                dragTargetIdx < static_cast<int>(fxNodes.size())) {
                std::swap(fxNodes[dragSourceIdx], fxNodes[dragTargetIdx]);
                mFxStackOrder[mSelectedNode] = fxNodes; // persist the new order
                dragSourceIdx = -1;
                dragTargetIdx = -1;
            }
        }
    }

    // Transform offsets display + reset (SceneObjectNode only)
    auto* selNode = Animator::instance() ? Animator::instance()->getRegisteredNode(mSelectedNode) : nullptr;
    if (selNode && selNode->getTypeName() == "SceneObjectNode") {
        auto* soNode = dynamic_cast<SceneObjectNode*>(selNode);
        if (soNode) {
            ImGui::Separator();

            // DAG Priority toggle
            bool dagPri = soNode->isDAGPriority();
            if (ImGui::Checkbox("DAG Priority", &dagPri)) {
                soNode->setDAGPriority(dagPri);
                soNode->onLinkChanged(); // refresh immediately
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("When ON, DAG animations override gizmo offsets on linked axes.\nWhen OFF, gizmo offsets apply fully on all axes.");

            auto op = soNode->getOffsetPos();
            auto or_ = soNode->getOffsetRot();
            auto os = soNode->getOffsetScale();
            bool hasOffset = (op != Ogre::Vector3::ZERO || or_ != Ogre::Vector3::ZERO ||
                              os != Ogre::Vector3::UNIT_SCALE);
            if (hasOffset) {
                ImGui::TextDisabled("Transform Offsets");
                // Position
                ImGui::Text("Pos:   %.2f, %.2f, %.2f", op.x, op.y, op.z);
                ImGui::SameLine();
                if (ImGui::SmallButton("X##resetPos")) soNode->resetOffsetPos();
                // Rotation
                ImGui::Text("Rot:   %.1f, %.1f, %.1f", or_.x, or_.y, or_.z);
                ImGui::SameLine();
                if (ImGui::SmallButton("X##resetRot")) soNode->resetOffsetRot();
                // Scale
                ImGui::Text("Scale: %.2f, %.2f, %.2f", os.x, os.y, os.z);
                ImGui::SameLine();
                if (ImGui::SmallButton("X##resetScl")) soNode->resetOffsetScale();
                // Global reset
                if (ImGui::SmallButton("Reset All")) soNode->resetOffsets();
            }
        }
    }

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

    const std::string nodeType = node->getTypeName();

    for (auto& param : spec->getParams()) {
        const std::string& label = param.displayLabel();
        std::string id = "##ps_" + param.name;

        // v3.5 Lot C: plugin-contributed custom widget hook. If a plugin (via
        // bbfx.ui.registerInspectorWidget, landing in Lot P) registered a
        // widget for this node-type/port, let it draw and skip the built-in
        // widget below. The registry is empty by default, so builtins are
        // unaffected.
        if (InspectorWidgetRegistry::instance().tryDraw(
                nodeType, mSelectedNode, param.name, param)) {
            continue;
        }

        switch (param.type) {
            case ParamType::FLOAT: {
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine(120.0f);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::SliderFloat(id.c_str(), &param.floatVal, param.minVal, param.maxVal)) {
                    // Sync to DAG port if exists (tooltip shown below)
                    auto& inputs = node->getInputs();
                    auto it = inputs.find(param.name);
                    if (it != inputs.end()) {
                        it->second->setValue(param.floatVal);
                        // Record to automation if recording
                        if (mIsRecording && mRecordValueCb) {
                            mRecordValueCb(mSelectedNode, param.name, param.floatVal, mCurrentBeat);
                        }
                    }
                }
                // Right-click context menu: Add to Timeline
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    ImGui::OpenPopup(("##paramCtx_" + param.name).c_str());
                }
                if (ImGui::BeginPopup(("##paramCtx_" + param.name).c_str())) {
                    if (ImGui::MenuItem("Add to Timeline") && mAddToTimelineCb) {
                        mAddToTimelineCb(mSelectedNode, param.name, param.minVal, param.maxVal);
                    }
                    ImGui::Separator();
                    {
                        auto& mlm = MidiLearnManager::instance();
                        bool isLearning = mlm.isLearning() &&
                            mlm.getLearnTarget().type == "port" &&
                            mlm.getLearnTarget().nodeName == mSelectedNode &&
                            mlm.getLearnTarget().portName == param.name;
                        if (isLearning) {
                            if (ImGui::MenuItem("Cancel MIDI Learn")) {
                                mlm.cancelLearn();
                            }
                        } else {
                            if (ImGui::MenuItem("MIDI Learn")) {
                                MidiLearnTarget target;
                                target.type = "port";
                                target.nodeName = mSelectedNode;
                                target.portName = param.name;
                                mlm.startLearn(target);
                            }
                        }
                        // Show current MIDI binding if any
                        for (size_t bi = 0; bi < mlm.getBindings().size(); ++bi) {
                            auto& b = mlm.getBindings()[bi];
                            if (b.target.type == "port" && b.target.nodeName == mSelectedNode &&
                                b.target.portName == param.name) {
                                ImGui::TextDisabled("MIDI: %s#%d ch%d",
                                    b.midiType.c_str(), b.number, b.channel);
                                if (ImGui::MenuItem("Clear MIDI Binding")) {
                                    mlm.removeBinding(static_cast<int>(bi));
                                }
                                break;
                            }
                        }
                    }
                    ImGui::EndPopup();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s [%.2f - %.2f] (right-click: Add to Timeline)",
                        param.name.c_str(), param.minVal, param.maxVal);
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
            case ParamType::SHADER: {
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
            case ParamType::TEXTURE: {
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine(120.0f);
                std::string btnLbl = param.stringVal.empty() ? "(none)" : param.stringVal;
                if (btnLbl.size() > 20) btnLbl = btnLbl.substr(0, 19) + "~";
                std::string popupId = "TexPicker##" + param.name;
                if (mThumbCache) {
                    ImTextureID thumb = mThumbCache->getThumbnail(param.stringVal);
                    ImGui::Image(thumb, {20, 20}); ImGui::SameLine();
                }
                if (ImGui::Button((btnLbl + "##btn" + param.name).c_str())) {
                    ImGui::OpenPopup(popupId.c_str());
                    mPickerSearch[0] = '\0';
                    // Save original texture for preview restore
                    mPreviewOriginalTexture = param.stringVal;
                    mPreviewActive = false;
                    mPreviewCurrentTexture.clear();
                }
                if (ImGui::BeginPopup(popupId.c_str())) {
                    ImGui::InputTextWithHint("##texSearch", "Search...", mPickerSearch, sizeof(mPickerSearch));
                    auto textures = ResourceEnumerator::listTextures();
                    std::string query = mPickerSearch;
                    std::transform(query.begin(), query.end(), query.begin(), ::tolower);
                    bool anyHovered = false;
                    ImGui::BeginChild("##texGrid", {320, 280});
                    float panelW = ImGui::GetContentRegionAvail().x;
                    for (auto& t : textures) {
                        if (!query.empty()) {
                            std::string lower = t;
                            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                            if (lower.find(query) == std::string::npos) continue;
                        }
                        if (mThumbCache) {
                            ImTextureID th = mThumbCache->getThumbnail(t);
                            bool isCurrent = (t == param.stringVal);
                            if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Border, {1.0f, 0.6f, 0.0f, 1.0f});
                            ImGui::BeginGroup();
                            ImGui::Image(th, {48, 48});
                            if (ImGui::IsItemClicked()) {
                                // Commit selection — no restore needed
                                param.stringVal = t;
                                mPreviewActive = false;
                                ImGui::CloseCurrentPopup();
                            }
                            if (ImGui::IsItemHovered()) {
                                anyHovered = true;
                                // Preview live: temporarily set the texture on the object
                                if (t != mPreviewCurrentTexture) {
                                    param.stringVal = t;
                                    mPreviewCurrentTexture = t;
                                    mPreviewActive = true;
                                }
                                ImGui::BeginTooltip();
                                ImGui::Image(th, {128, 128});
                                ImGui::Text("%s", t.c_str());
                                ImGui::EndTooltip();
                            }
                            ImGui::EndGroup();
                            if (isCurrent) ImGui::PopStyleColor();
                            float nextX = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + 4 + 48;
                            if (nextX < panelW) ImGui::SameLine(0, 4);
                        } else {
                            if (ImGui::Selectable(t.c_str(), t == param.stringVal)) {
                                param.stringVal = t;
                                mPreviewActive = false;
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }
                    // If nothing hovered and preview was active, restore original
                    if (!anyHovered && mPreviewActive) {
                        param.stringVal = mPreviewOriginalTexture;
                        mPreviewActive = false;
                        mPreviewCurrentTexture.clear();
                    }
                    ImGui::EndChild();
                    ImGui::EndPopup();
                } else {
                    // Popup closed (escape or click outside) — restore if preview was active
                    if (mPreviewActive) {
                        param.stringVal = mPreviewOriginalTexture;
                        mPreviewActive = false;
                        mPreviewCurrentTexture.clear();
                    }
                }
                break;
            }
            case ParamType::MATERIAL:
            case ParamType::PARTICLE:
            case ParamType::COMPOSITOR: {
                ImGui::Text("%s", label.c_str());
                ImGui::SameLine(120.0f);
                std::string btnLbl2 = param.stringVal.empty() ? "(none)" : param.stringVal;
                if (btnLbl2.size() > 24) btnLbl2 = btnLbl2.substr(0, 23) + "~";
                std::string popId = "Picker##" + param.name;
                if (ImGui::Button((btnLbl2 + "##btn" + param.name).c_str())) {
                    ImGui::OpenPopup(popId.c_str());
                    mPickerSearch[0] = '\0';
                }
                if (ImGui::BeginPopup(popId.c_str())) {
                    ImGui::InputTextWithHint("##pickSearch", "Search...", mPickerSearch, sizeof(mPickerSearch));
                    std::vector<std::string> items;
                    if (param.type == ParamType::MATERIAL) items = ResourceEnumerator::listMaterials();
                    else if (param.type == ParamType::PARTICLE) items = ResourceEnumerator::listParticleTemplates();
                    else items = ResourceEnumerator::listCompositors();
                    std::string q = mPickerSearch;
                    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                    ImGui::BeginChild("##pickList", {250, 200});
                    for (auto& item : items) {
                        if (!q.empty()) {
                            std::string lower = item;
                            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                            if (lower.find(q) == std::string::npos) continue;
                        }
                        bool sel = (item == param.stringVal);
                        if (ImGui::Selectable(item.c_str(), sel)) {
                            param.stringVal = item;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::EndChild();
                    ImGui::EndPopup();
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

void InspectorPanel::syncFxOrder(const std::string& soNode, std::vector<std::string>& fxNodes) {
    auto it = mFxStackOrder.find(soNode);
    if (it == mFxStackOrder.end()) {
        // No persisted order — store current
        mFxStackOrder[soNode] = fxNodes;
        return;
    }

    auto& order = it->second;
    // Build ordered result: persisted order first (if still valid), then new ones at end
    std::vector<std::string> result;
    std::set<std::string> fxSet(fxNodes.begin(), fxNodes.end());
    for (auto& name : order) {
        if (fxSet.count(name)) {
            result.push_back(name);
            fxSet.erase(name);
        }
    }
    // Append newly added FX
    for (auto& name : fxNodes) {
        if (fxSet.count(name)) result.push_back(name);
    }
    fxNodes = result;
    order = result;
}

} // namespace bbfx
