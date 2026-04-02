#include "NodeEditorPanel.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/AnimationPort.h"
#include "../../core/PrimitiveNodes.h"
#include "../NodeTypeRegistry.h"
#include "../commands/CommandManager.h"
#include "../commands/NodeCommands.h"
#include "../commands/LinkCommands.h"
#include "../commands/EditCommands.h"

#include <imgui.h>
#include <imgui_node_editor.h>
#include <imgui_node_editor_internal.h>
#include <sol/sol.hpp>
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cfloat>
#include <set>

namespace ned = ax::NodeEditor;

namespace bbfx {

// ── Color map by node type name ───────────────────────────────────────────────
static const std::map<std::string, ImVec4> kNodeColors = {
    {"RootTimeNode",            {0.00f, 0.67f, 0.00f, 1.0f}}, // green
    {"LuaAnimationNode",        {0.87f, 0.87f, 0.87f, 1.0f}}, // white
    {"KeyboardManager",         {0.67f, 0.67f, 0.00f, 1.0f}}, // yellow
    {"MouseManager",            {0.67f, 0.67f, 0.00f, 1.0f}},
    {"JoystickManager",         {0.67f, 0.67f, 0.00f, 1.0f}},
    {"AudioCapture",            {1.00f, 0.53f, 0.00f, 1.0f}}, // orange
    {"AudioAnalyzerNode",       {1.00f, 0.53f, 0.00f, 1.0f}},
    {"BeatDetectorNode",        {1.00f, 0.53f, 0.00f, 1.0f}},
    {"LFONode",                 {0.00f, 0.53f, 1.00f, 1.0f}}, // blue
    {"RampNode",                {0.00f, 0.53f, 1.00f, 1.0f}},
    {"DelayNode",               {0.00f, 0.53f, 1.00f, 1.0f}},
    {"EnvelopeFollowerNode",    {0.00f, 0.53f, 1.00f, 1.0f}},
    {"PerlinFxNode",            {0.53f, 0.00f, 1.00f, 1.0f}}, // violet
    {"ShaderFxNode",            {0.53f, 0.00f, 1.00f, 1.0f}},
    {"TextureBlitter",          {0.53f, 0.00f, 1.00f, 1.0f}},
    {"WaveVertexShader",        {0.53f, 0.00f, 1.00f, 1.0f}},
    {"ColorShiftNode",          {0.53f, 0.00f, 1.00f, 1.0f}},
    {"TheoraClip",              {1.00f, 0.00f, 0.00f, 1.0f}}, // red
    {"TheoraClipNode",          {1.00f, 0.00f, 0.00f, 1.0f}},
    {"SubgraphNode",            {0.53f, 0.53f, 0.53f, 1.0f}}, // grey
    {"AccumulatorNode",         {0.40f, 0.40f, 0.40f, 1.0f}},
};

ImVec4 NodeEditorPanel::nodeColor(const std::string& typeName) const {
    auto it = kNodeColors.find(typeName);
    if (it != kNodeColors.end()) return it->second;
    return {0.87f, 0.87f, 0.87f, 1.0f}; // default white
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

NodeEditorPanel::NodeEditorPanel(sol::state& lua) : mLua(lua) {
    ned::Config cfg;
    cfg.SettingsFile = ""; // Don't persist to file — positions managed by .bbfx-project
    mEditorContext = ned::CreateEditor(&cfg);
}

NodeEditorPanel::~NodeEditorPanel() {
    if (mEditorContext) {
        ned::DestroyEditor(mEditorContext);
        mEditorContext = nullptr;
    }
}

// ── Sync DAG → editor ─────────────────────────────────────────────────────────

void NodeEditorPanel::syncFromDAG() {
    auto* animator = Animator::instance();
    if (!animator) return;

    auto names = animator->getRegisteredNodeNames();

    // Remove nodes no longer in DAG — also track their editor IDs for ned cleanup
    std::vector<std::string> toRemove;
    for (auto& [name, _] : mNodes) {
        bool found = false;
        for (auto& n : names) if (n == name) { found = true; break; }
        if (!found) toRemove.push_back(name);
    }
    for (auto& name : toRemove) {
        auto it = mNodes.find(name);
        if (it != mNodes.end()) {
            mStaleNodeIds.push_back(it->second.id);
            // Also collect pin IDs
            for (auto& pin : it->second.inputPins) mStalePinIds.push_back(pin);
            for (auto& pin : it->second.outputPins) mStalePinIds.push_back(pin);
        }
        mNodes.erase(name);
    }

    // Add new nodes (links are synced afterwards in syncLinksFromDAG)
    for (auto& name : names) {
        if (mNodes.count(name)) continue;
        if (name.rfind("shell/", 0) == 0) continue; // hide internal TCP shell node

        auto* node = animator->getRegisteredNode(name);
        if (!node) continue;

        NodeData nd;
        nd.name     = name;
        nd.typeName = node->getTypeName(); // requires getTypeName() in AnimationNode
        nd.id       = ned::NodeId(allocId());

        for (auto& [pname, _] : node->getInputs()) {
            nd.inputNames.push_back(pname);
            nd.inputPins.push_back(ned::PinId(allocId()));
        }
        for (auto& [pname, _] : node->getOutputs()) {
            nd.outputNames.push_back(pname);
            nd.outputPins.push_back(ned::PinId(allocId()));
        }

        mNodes[name] = std::move(nd);
    }

}

// ── Pin lookup helper ─────────────────────────────────────────────────────────

ned::PinId NodeEditorPanel::findPin(const std::string& nodeName,
                                     const std::string& portName,
                                     bool output) const {
    auto it = mNodes.find(nodeName);
    if (it == mNodes.end()) return ned::PinId(0);
    auto& nd = it->second;
    if (output) {
        for (size_t i = 0; i < nd.outputNames.size(); ++i)
            if (nd.outputNames[i] == portName) return nd.outputPins[i];
    } else {
        for (size_t i = 0; i < nd.inputNames.size(); ++i)
            if (nd.inputNames[i] == portName) return nd.inputPins[i];
    }
    return ned::PinId(0);
}

// ── Sync links from DAG edges ─────────────────────────────────────────────────

void NodeEditorPanel::syncLinksFromDAG() {
    auto* animator = Animator::instance();
    if (!animator) return;

    auto dagLinks = animator->getLinks();

    // Build a set of existing link keys for fast lookup
    auto linkKey = [](const std::string& fn, const std::string& fp,
                      const std::string& tn, const std::string& tp) {
        return fn + "\0" + fp + "\0" + tn + "\0" + tp;
    };

    std::set<std::string> existingKeys;
    for (auto& lk : mLinks)
        existingKeys.insert(linkKey(lk.fromNode, lk.fromPort, lk.toNode, lk.toPort));

    // Build a set of DAG keys to detect stale links
    std::set<std::string> dagKeys;
    for (auto& dl : dagLinks)
        dagKeys.insert(linkKey(dl.fromNode, dl.fromPort, dl.toNode, dl.toPort));

    // Remove stale links (present in mLinks but no longer in DAG)
    mLinks.erase(
        std::remove_if(mLinks.begin(), mLinks.end(), [&](const LinkData& lk) {
            return dagKeys.find(linkKey(lk.fromNode, lk.fromPort, lk.toNode, lk.toPort))
                   == dagKeys.end();
        }),
        mLinks.end());

    // Add new links from DAG
    for (auto& dl : dagLinks) {
        auto key = linkKey(dl.fromNode, dl.fromPort, dl.toNode, dl.toPort);
        if (existingKeys.count(key)) continue;

        auto startPin = findPin(dl.fromNode, dl.fromPort, true);
        auto endPin   = findPin(dl.toNode,   dl.toPort,   false);
        if (!startPin || !endPin) continue;

        LinkData lk;
        lk.id       = ned::LinkId(allocId());
        lk.startPin = startPin;
        lk.endPin   = endPin;
        lk.fromNode = dl.fromNode;
        lk.fromPort = dl.fromPort;
        lk.toNode   = dl.toNode;
        lk.toPort   = dl.toPort;
        mLinks.push_back(lk);
    }
}

// ── Render ────────────────────────────────────────────────────────────────────

void NodeEditorPanel::render() {
    ImGui::Begin("Node Editor", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (!mEditorContext) {
        ImGui::TextDisabled("(Node editor context not initialized)");
        ImGui::End();
        return;
    }

    syncFromDAG();
    syncLinksFromDAG();

    ned::SetCurrentEditor(mEditorContext);
    ned::Begin("DAG", {0, 0});

    // Clean up stale node/pin IDs from deferred deletes
    if (!mStaleNodeIds.empty()) {
        for (auto& id : mStaleNodeIds) ned::DeleteNode(id);
        mStaleNodeIds.clear();
    }
    if (!mStalePinIds.empty()) {
        mStalePinIds.clear(); // pins are cleaned with their node
    }
    // Also clean stale links referencing deleted nodes
    {
        std::set<std::string> validNodes;
        for (auto& [name, _] : mNodes) validNodes.insert(name);
        mLinks.erase(
            std::remove_if(mLinks.begin(), mLinks.end(), [&](const LinkData& lk) {
                return validNodes.find(lk.fromNode) == validNodes.end() ||
                       validNodes.find(lk.toNode) == validNodes.end();
            }),
            mLinks.end());
    }


    // Convert deferred drop screen position to canvas coords (ned context is active here)
    if (!mDropPresetName.empty()) {
        auto canvasPos = ned::ScreenToCanvas(mDropScreenPos);
        // For multi-node presets, node names are "presetName_xxx".
        // Queue positions for all nodes that match the prefix.
        std::string prefix = mDropPresetName + "_";
        float yOff = 0.0f;
        bool anyMatch = false;
        for (auto& [name, nd] : mNodes) {
            if (name == mDropPresetName || name.rfind(prefix, 0) == 0) {
                mPendingPositions.push_back({name, canvasPos.x, canvasPos.y + yOff});
                yOff += 150.0f;
                anyMatch = true;
            }
        }
        if (!anyMatch) {
            // Nodes not yet synced — queue with prefix marker for retry
            mPendingPositions.push_back({mDropPresetName, canvasPos.x, canvasPos.y});
        }
        mDropPresetName.clear();
    }

    // Apply pending positions (deferred — needs active editor context after ned::Begin)
    if (!mPendingPositions.empty()) {
        std::vector<NodePosition> remaining;
        for (auto& np : mPendingPositions) {
            auto it = mNodes.find(np.name);
            if (it != mNodes.end()) {
                ned::SetNodePosition(it->second.id, {np.x, np.y});
            } else {
                // Check if this is a preset prefix — expand to matching nodes
                std::string prefix = np.name + "_";
                float yOff = 0.0f;
                bool expanded = false;
                for (auto& [name, nd] : mNodes) {
                    if (name.rfind(prefix, 0) == 0) {
                        ned::SetNodePosition(nd.id, {np.x, np.y + yOff});
                        yOff += 150.0f;
                        expanded = true;
                    }
                }
                if (!expanded) {
                    remaining.push_back(np); // retry next frame
                }
            }
        }
        mPendingPositions = std::move(remaining);
    }

    auto* animator = Animator::instance();

    // ── Draw nodes ────────────────────────────────────────────────────────────
    for (auto& [name, nd] : mNodes) {
        auto* node = animator ? animator->getRegisteredNode(name) : nullptr;

        ImVec4 col = nodeColor(nd.typeName);
        bool nodeEnabled = true;
        if (node) nodeEnabled = node->isEnabled();

        // Gray out disabled nodes
        if (!nodeEnabled) {
            col = {0.35f, 0.35f, 0.35f, 1.0f};
            ned::PushStyleColor(ned::StyleColor_NodeBg, ImVec4(0.15f, 0.15f, 0.15f, 0.8f));
        }
        ned::PushStyleColor(ned::StyleColor_NodeBorder, col);
        ned::BeginNode(nd.id);

        // Title bar
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        if (!nodeEnabled) {
            ImGui::Text("[OFF] %s", nd.typeName.c_str());
        } else {
            ImGui::TextUnformatted(nd.typeName.c_str());
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("  %s", name.c_str());

        // FX badge for SceneObjectNode — show count of connected FX nodes
        if (nd.typeName == "SceneObjectNode" && node) {
            int fxCount = 0;
            std::string fxTooltip;
            auto& outputs = node->getOutputs();
            auto entityIt = outputs.find("entity");
            if (entityIt != outputs.end()) {
                auto* animator = Animator::instance();
                if (animator) {
                    for (auto& n2 : animator->getRegisteredNodeNames()) {
                        auto* other = animator->getRegisteredNode(n2);
                        if (!other || other == node) continue;
                        auto& otherInputs = other->getInputs();
                        auto otherEntityIt = otherInputs.find("entity");
                        if (otherEntityIt != otherInputs.end()) {
                            if (other->getParamSpec()) {
                                auto* te = other->getParamSpec()->getParam("target_entity");
                                if (te && te->stringVal == name) {
                                    fxCount++;
                                    if (!fxTooltip.empty()) fxTooltip += "\n";
                                    fxTooltip += n2;
                                }
                            }
                        }
                    }
                }
            }
            if (fxCount > 0) {
                ImGui::SameLine();
                ImGui::TextColored({0.5f, 1.0f, 0.5f, 1.0f}, "FX:%d", fxCount);
                if (ImGui::IsItemHovered() && !fxTooltip.empty()) {
                    ImGui::SetTooltip("%s", fxTooltip.c_str());
                }
            }
        }

        // Input pins (left)
        for (size_t i = 0; i < nd.inputPins.size(); ++i) {
            ned::BeginPin(nd.inputPins[i], ned::PinKind::Input);
            ImGui::Text("> %s", nd.inputNames[i].c_str()); // ● bullet
            // Show current port value
            if (node) {
                auto& inputs = node->getInputs();
                auto it = inputs.find(nd.inputNames[i]);
                if (it != inputs.end()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("%.3f", it->second->getValue());
                }
            }
            ned::EndPin();
        }

        // Output pins (right)
        for (size_t i = 0; i < nd.outputPins.size(); ++i) {
            ned::BeginPin(nd.outputPins[i], ned::PinKind::Output);
            ImGui::Text("%s >", nd.outputNames[i].c_str());
            if (node) {
                auto& outputs = node->getOutputs();
                auto it = outputs.find(nd.outputNames[i]);
                if (it != outputs.end()) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("%.3f", it->second->getValue());
                }
            }
            ned::EndPin();
        }

        ned::EndNode();
        ned::PopStyleColor(); // NodeBorder
        if (!nodeEnabled) ned::PopStyleColor(); // NodeBg
    }

    // ── Draw links ────────────────────────────────────────────────────────────
    for (auto& lk : mLinks) {
        ned::Link(lk.id, lk.startPin, lk.endPin);
    }

    // ── Handle new link creation ──────────────────────────────────────────────
    handleLinkCreation();

    // ── Handle deletion (single BeginDelete/EndDelete scope) ──────────────────
    // ── Delete key: delete selected node via CommandManager (undoable) ─────
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !mSelectedNode.empty()) {
        if (mNodes.count(mSelectedNode)) {
            CommandManager::instance().execute(
                std::make_unique<DeleteNodeCommand>(mSelectedNode, mLua));
            mSelectedNode.clear();
        }
    }

    handleDeletion();

    // ── Context menu ──────────────────────────────────────────────────────────
    showNodeContextMenu();

    // ── Flow animation on links ─────────────────────────────────────────────
    for (auto& lk : mLinks) {
        ned::Flow(lk.id);
    }

    // ── Ctrl+D: duplicate selected nodes ──────────────────────────────────────
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        static int dupCounter = 0;
        std::vector<ned::NodeId> selectedIds(ned::GetSelectedObjectCount());
        int count = ned::GetSelectedNodes(selectedIds.data(), static_cast<int>(selectedIds.size()));
        for (int i = 0; i < count; ++i) {
            for (auto& [name, nd] : mNodes) {
                if (nd.id == selectedIds[i]) {
                    std::string newName = name + "_dup" + std::to_string(++dupCounter);
                    CommandManager::instance().execute(
                        std::make_unique<CreateNodeCommand>(nd.typeName, newName, mLua));
                    break;
                }
            }
        }
    }

    // ── Bookmarks: Ctrl+1..9 save, 1..9 restore ──────────────────────────────
    // Saves view origin + zoom via public GetView(). Restores via NavigateTo().
    {
        auto* edCtx = reinterpret_cast<ax::NodeEditor::Detail::EditorContext*>(mEditorContext);
        for (int k = 0; k < 9; ++k) {
            ImGuiKey key = static_cast<ImGuiKey>(static_cast<int>(ImGuiKey_1) + k);
            if (ImGui::IsKeyPressed(key) && !ImGui::GetIO().KeyAlt && !ImGui::IsAnyItemActive()) {
                if (ImGui::GetIO().KeyCtrl) {
                    // Save: visible canvas rect
                    auto vr = edCtx->GetViewRect();
                    mBookmarkRects[k][0] = vr.Min.x;
                    mBookmarkRects[k][1] = vr.Min.y;
                    mBookmarkRects[k][2] = vr.Max.x;
                    mBookmarkRects[k][3] = vr.Max.y;
                    mBookmarkSet[k] = true;
                } else if (mBookmarkSet[k]) {
                    // Restore: navigate to saved view rect with zoom
                    ImRect target(
                        {mBookmarkRects[k][0], mBookmarkRects[k][1]},
                        {mBookmarkRects[k][2], mBookmarkRects[k][3]});
                    edCtx->NavigateTo(target, true, -1.0f);
                }
            }
        }
    }

    // ── Auto-fit view after nodes are drawn (delay 3 frames for layout) ────
    static int navigateCountdown = 3;
    if (navigateCountdown > 0 && !mNodes.empty()) {
        --navigateCountdown;
        if (navigateCountdown == 0) {
            ned::NavigateToContent(0.5f);
        }
    }

    // ── Selection tracking (must be inside Begin/End scope) ─────────────────
    {
        ned::NodeId selectedId;
        if (ned::GetSelectedNodes(&selectedId, 1) == 1) {
            for (auto& [name, nd] : mNodes) {
                if (nd.id == selectedId && name != mSelectedNode) {
                    mSelectedNode = name;
                    if (mSelectionCallback) mSelectionCallback(name);
                }
            }
        }
    }

    ned::End();
    ned::SetCurrentEditor(nullptr);

    // ── Save as Preset modal ─────────────────────────────────────────────────
    if (mShowSavePresetDialog) {
        ImGui::OpenPopup("Save as Preset");
        mShowSavePresetDialog = false;
    }
    if (ImGui::BeginPopupModal("Save as Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Preset name:");
        ImGui::SetNextItemWidth(250.0f);
        ImGui::InputText("##preset_name", mPresetNameBuf, sizeof(mPresetNameBuf));

        ImGui::Separator();
        if (ImGui::Button("Save", {100, 0})) {
            std::string presetName(mPresetNameBuf);
            if (!presetName.empty()) {
                std::string path = "lua/presets/" + presetName + ".lua";
                std::ofstream ofs(path);
                if (ofs.is_open()) {
                    // Find the node type for the preset
                    std::string typeName = "LuaAnimationNode";
                    auto* animator = Animator::instance();
                    if (animator && !mSelectedNode.empty()) {
                        auto* node = animator->getRegisteredNode(mSelectedNode);
                        if (node) typeName = node->getTypeName();
                    }

                    ofs << "-- " << presetName << ".lua -- BBFx Preset\n\n";
                    ofs << "return {\n";
                    ofs << "    name = \"" << presetName << "\",\n";
                    ofs << "    description = \"\",\n";
                    ofs << "    nodes = {\n";
                    ofs << "        { type = \"" << typeName << "\", name = \"" << presetName << "\", params = {} },\n";
                    ofs << "    }\n";
                    ofs << "}\n";
                    ofs.close();
                    std::cout << "[NodeEditor] Saved preset to " << path << std::endl;
                } else {
                    std::cout << "[NodeEditor] Failed to write preset file: " << path << std::endl;
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {100, 0})) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Drag-drop target for presets ─────────────────────────────────────
    if (ImGui::BeginDragDropTarget()) {
        if (auto* payload = ImGui::AcceptDragDropPayload("PRESET_NAME")) {
            std::string presetName(static_cast<const char*>(payload->Data));
            // Save drop screen position — will be converted to canvas in next frame
            mDropScreenPos = ImGui::GetMousePos();
            mDropPresetName = presetName;
            // Use the deferred debugger queue to avoid Lua re-entrancy crash
            sol::optional<sol::table> dbg = mLua["dbg"];
            if (dbg) {
                sol::optional<sol::function> presetFn = (*dbg)["preset"];
                if (presetFn) {
                    (*presetFn)(presetName);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
}

// ── Link creation: drag pin → pin ─────────────────────────────────────────────

void NodeEditorPanel::handleLinkCreation() {
    if (!ned::BeginCreate()) return;

    ned::PinId startPinId, endPinId;
    if (ned::QueryNewLink(&startPinId, &endPinId)) {
        if (startPinId && endPinId) {
            // Find which nodes and ports these pins belong to
            std::string fromNode, fromPort, toNode, toPort;
            for (auto& [name, nd] : mNodes) {
                for (size_t i = 0; i < nd.outputPins.size(); ++i) {
                    if (nd.outputPins[i] == startPinId) {
                        fromNode = name;
                        fromPort = nd.outputNames[i];
                    }
                }
                for (size_t i = 0; i < nd.inputPins.size(); ++i) {
                    if (nd.inputPins[i] == endPinId) {
                        toNode = name;
                        toPort = nd.inputNames[i];
                    }
                }
            }

            // Handle reverse drag direction (input → output)
            if (fromNode.empty() || toNode.empty()) {
                fromNode.clear(); fromPort.clear();
                toNode.clear(); toPort.clear();
                for (auto& [name, nd] : mNodes) {
                    for (size_t i = 0; i < nd.outputPins.size(); ++i) {
                        if (nd.outputPins[i] == endPinId) {
                            fromNode = name;
                            fromPort = nd.outputNames[i];
                        }
                    }
                    for (size_t i = 0; i < nd.inputPins.size(); ++i) {
                        if (nd.inputPins[i] == startPinId) {
                            toNode = name;
                            toPort = nd.inputNames[i];
                        }
                    }
                }
            }

            bool valid = !fromNode.empty() && !toNode.empty() && fromNode != toNode;
            if (valid) {
                // AcceptNewItem returns true only on mouse release (link confirmed)
                if (ned::AcceptNewItem({0.0f, 1.0f, 0.0f, 1.0f})) {
                    CommandManager::instance().execute(
                        std::make_unique<CreateLinkCommand>(fromNode, fromPort, toNode, toPort));
                    // ParamSpec fill + onLinkChanged() handled by linkPorts() inside the command
                }
            } else {
                ned::RejectNewItem({1.0f, 0.0f, 0.0f, 1.0f}); // red = reject
            }
        }
    }
    ned::EndCreate();
}

// Handles both link and node deletion in a single BeginDelete/EndDelete scope.
// imgui-node-editor requires exactly one BeginDelete/EndDelete pair per frame.
void NodeEditorPanel::handleDeletion() {
    if (!ned::BeginDelete()) return;

    // ── Link deletion ─────────────────────────────────────────────────────────
    ned::LinkId deletedLinkId;
    while (ned::QueryDeletedLink(&deletedLinkId)) {
        if (ned::AcceptDeletedItem()) {
            for (auto it = mLinks.begin(); it != mLinks.end(); ++it) {
                if (it->id == deletedLinkId) {
                    // ParamSpec clear + onLinkChanged() handled by unlinkPorts() inside the command
                    CommandManager::instance().execute(
                        std::make_unique<DeleteLinkCommand>(
                            it->fromNode, it->fromPort, it->toNode, it->toPort));
                    mLinks.erase(it);
                    break;
                }
            }
        }
    }

    // ── Node deletion ─────────────────────────────────────────────────────────
    // Do NOT use ned::QueryDeletedNode/AcceptDeletedItem — it marks the node
    // as dead inside ned, causing ned::End() to crash when the node is still
    // in the Begin/End scope. Instead, reject all node deletions from ned and
    // handle them ourselves via gPendingDeletes.
    ned::NodeId deletedNodeId;
    while (ned::QueryDeletedNode(&deletedNodeId)) {
        ned::RejectDeletedItem(); // tell ned "no, don't delete it internally"
    }

    ned::EndDelete();
}

void NodeEditorPanel::showNodeContextMenu() {
    ned::Suspend();
    if (ned::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("NodeCreationMenu");
        mCreateMenuPos = ImGui::GetMousePos();
    }
    if (ImGui::BeginPopup("NodeCreationMenu")) {
        ImGui::TextDisabled("Create Node");
        ImGui::Separator();

        static int counter = 0;
        auto byCategory = NodeTypeRegistry::instance().getByCategory();
        for (auto& [category, types] : byCategory) {
            if (ImGui::BeginMenu(category.c_str())) {
                for (auto* info : types) {
                    if (ImGui::MenuItem(info->typeName.c_str())) {
                        std::string name = info->typeName + "_" + std::to_string(++counter);
                        CommandManager::instance().execute(
                            std::make_unique<CreateNodeCommand>(info->typeName, name, mLua));
                        // Position the new node at the right-click location
                        auto canvasPos = ned::ScreenToCanvas(mCreateMenuPos);
                        mPendingPositions.push_back({name, canvasPos.x, canvasPos.y});
                    }
                }
                ImGui::EndMenu();
            }
        }

        ImGui::EndPopup();
    }

    // ── Link context menu (right-click on a link) ─────────────────────────────
    ned::LinkId contextLinkId;
    if (ned::ShowLinkContextMenu(&contextLinkId)) {
        mContextLinkId = contextLinkId;
        ImGui::OpenPopup("LinkContextMenu");
    }
    if (ImGui::BeginPopup("LinkContextMenu")) {
        if (ImGui::MenuItem("Delete Link")) {
            for (auto it = mLinks.begin(); it != mLinks.end(); ++it) {
                if (it->id == mContextLinkId) {
                    // ParamSpec clear + onLinkChanged() handled by unlinkPorts() inside the command
                    CommandManager::instance().execute(
                        std::make_unique<DeleteLinkCommand>(
                            it->fromNode, it->fromPort, it->toNode, it->toPort));
                    mLinks.erase(it);
                    break;
                }
            }
        }
        ImGui::EndPopup();
    }

    // ── Node context menu (right-click on a node) ────────────────────────────
    ned::NodeId contextNodeId;
    if (ned::ShowNodeContextMenu(&contextNodeId)) {
        // Auto-select the right-clicked node so the context menu operates on it
        for (auto& [name, nd] : mNodes) {
            if (nd.id == contextNodeId) {
                mSelectedNode = name;
                ned::SelectNode(contextNodeId);
                if (mSelectionCallback) mSelectionCallback(name);
                break;
            }
        }
        ImGui::OpenPopup("NodeContextMenu");
    }
    if (ImGui::BeginPopup("NodeContextMenu")) {
        // Enable/Disable toggle
        if (!mSelectedNode.empty()) {
            auto* animator = Animator::instance();
            auto* node = animator ? animator->getRegisteredNode(mSelectedNode) : nullptr;
            if (node) {
                bool en = node->isEnabled();
                if (ImGui::MenuItem(en ? "Disable" : "Enable")) {
                    CommandManager::instance().execute(
                        std::make_unique<SetEnabledCommand>(mSelectedNode, en, !en));
                }
                ImGui::Separator();
            }
        }
        if (ImGui::MenuItem("Save as Preset")) {
            mShowSavePresetDialog = true;
            std::memset(mPresetNameBuf, 0, sizeof(mPresetNameBuf));
            if (!mSelectedNode.empty()) {
                std::strncpy(mPresetNameBuf, mSelectedNode.c_str(), sizeof(mPresetNameBuf) - 1);
            }
        }
        ImGui::EndPopup();
    }

    ned::Resume();
}

std::vector<NodeEditorPanel::NodePosition> NodeEditorPanel::getNodePositions() const {
    std::vector<NodePosition> positions;
    // Don't change current editor if we're already inside a render scope
    auto* prevEditor = ned::GetCurrentEditor();
    if (!prevEditor) ned::SetCurrentEditor(mEditorContext);
    for (auto& [name, nd] : mNodes) {
        auto pos = ned::GetNodePosition(nd.id);
        positions.push_back({name, pos.x, pos.y});
    }
    if (!prevEditor) ned::SetCurrentEditor(nullptr);
    return positions;
}

void NodeEditorPanel::setNodePositions(const std::vector<NodePosition>& positions) {
    // Append positions — some nodes may not be in mNodes yet
    // (syncFromDAG hasn't run). Apply what we can now, defer the rest.
    mPendingPositions.insert(mPendingPositions.end(), positions.begin(), positions.end());

    ned::SetCurrentEditor(mEditorContext);
    for (auto& np : positions) {
        auto it = mNodes.find(np.name);
        if (it != mNodes.end()) {
            ned::SetNodePosition(it->second.id, {np.x, np.y});
        }
    }
    ned::SetCurrentEditor(nullptr);
}

} // namespace bbfx
