#include "NodeEditorPanel.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../../core/AnimationPort.h"
#include "../../core/PrimitiveNodes.h"

#include <imgui.h>
#include <imgui_node_editor.h>
#include <sol/sol.hpp>
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
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
    cfg.SettingsFile = "node_editor.json";
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

    // Remove nodes no longer in DAG
    std::vector<std::string> toRemove;
    for (auto& [name, _] : mNodes) {
        bool found = false;
        for (auto& n : names) if (n == name) { found = true; break; }
        if (!found) toRemove.push_back(name);
    }
    for (auto& name : toRemove) mNodes.erase(name);

    // Add new nodes (links are synced afterwards in syncLinksFromDAG)
    for (auto& name : names) {
        if (mNodes.count(name)) continue;

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

    auto* animator = Animator::instance();

    // ── Draw nodes ────────────────────────────────────────────────────────────
    for (auto& [name, nd] : mNodes) {
        auto* node = animator ? animator->getRegisteredNode(name) : nullptr;

        ImVec4 col = nodeColor(nd.typeName);
        ned::PushStyleColor(ned::StyleColor_NodeBorder, col);
        ned::BeginNode(nd.id);

        // Title bar
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(nd.typeName.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("  %s", name.c_str());

        // Input pins (left)
        for (size_t i = 0; i < nd.inputPins.size(); ++i) {
            ned::BeginPin(nd.inputPins[i], ned::PinKind::Input);
            ImGui::Text("\xe2\x97\x8f %s", nd.inputNames[i].c_str()); // ● bullet
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
            ImGui::Text("%s \xe2\x97\x8f", nd.outputNames[i].c_str());
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
        ned::PopStyleColor();
    }

    // ── Draw links ────────────────────────────────────────────────────────────
    for (auto& lk : mLinks) {
        ned::Link(lk.id, lk.startPin, lk.endPin);
    }

    // ── Handle new link creation ──────────────────────────────────────────────
    handleLinkCreation();

    // ── Handle deletion (single BeginDelete/EndDelete scope) ──────────────────
    handleDeletion();

    // ── Context menu ──────────────────────────────────────────────────────────
    showNodeContextMenu();

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
            auto* animator = Animator::instance();
            if (animator) {
                std::string presetPath = "lua/presets/" + presetName + ".lua";
                auto result = mLua.safe_script_file(presetPath, sol::script_pass_on_error);
                if (result.valid()) {
                    sol::table preset = result;
                    std::string name = preset.get_or<std::string>("name", presetName);
                    sol::table nodes = preset.get_or<sol::table>("nodes", sol::nil);
                    if (nodes.valid()) {
                        for (auto& [k, v] : nodes) {
                            sol::table nodeDesc = v.as<sol::table>();
                            std::string nodeName = nodeDesc.get_or<std::string>("name", presetName);
                            // Create a LuaAnimationNode with no-op callback
                            sol::function noop = mLua.load("return function(node) end")().get<sol::function>();
                            auto* node = new LuaAnimationNode(nodeName, noop);
                            node->addInput("in");
                            node->addOutput("out");
                            animator->registerNode(node);
                        }
                    }
                    std::cout << "[NodeEditor] Preset '" << presetName << "' instantiated" << std::endl;
                } else {
                    sol::error err = result;
                    std::cout << "[NodeEditor] Preset load error: " << err.what() << std::endl;
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

            bool valid = !fromNode.empty() && !toNode.empty() && fromNode != toNode;
            if (valid) {
                // AcceptNewItem returns true only on mouse release (link confirmed)
                if (ned::AcceptNewItem({0.0f, 1.0f, 0.0f, 1.0f})) {
                    // Create the link in the DAG
                    auto* animator = Animator::instance();
                    if (animator) {
                        auto* from = animator->getRegisteredNode(fromNode);
                        auto* to   = animator->getRegisteredNode(toNode);
                        if (from && to) {
                            auto& outs = from->getOutputs();
                            auto& ins  = to->getInputs();
                            auto oit = outs.find(fromPort);
                            auto iit = ins.find(toPort);
                            if (oit != outs.end() && iit != ins.end()) {
                                animator->link(oit->second, iit->second);
                                // syncLinksFromDAG will pick up the new link next frame
                            }
                        }
                    }
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
                    auto* animator = Animator::instance();
                    if (animator) {
                        auto* from = animator->getRegisteredNode(it->fromNode);
                        auto* to   = animator->getRegisteredNode(it->toNode);
                        if (from && to) {
                            auto& outs = from->getOutputs();
                            auto& ins  = to->getInputs();
                            auto oit = outs.find(it->fromPort);
                            auto iit = ins.find(it->toPort);
                            if (oit != outs.end() && iit != ins.end()) {
                                animator->unlink(oit->second, iit->second);
                            }
                        }
                    }
                    mLinks.erase(it);
                    break;
                }
            }
        }
    }

    // ── Node deletion ─────────────────────────────────────────────────────────
    ned::NodeId deletedNodeId;
    while (ned::QueryDeletedNode(&deletedNodeId)) {
        std::string nodeName;
        for (auto& [name, nd] : mNodes) {
            if (nd.id == deletedNodeId) { nodeName = name; break; }
        }
        if (!nodeName.empty() && ned::AcceptDeletedItem()) {
            auto* animator = Animator::instance();
            if (animator) {
                auto* node = animator->getRegisteredNode(nodeName);
                if (node) animator->removeNode(node);
            }
            mNodes.erase(nodeName);
        }
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

        auto* animator = Animator::instance();
        static int counter = 0;

        if (ImGui::MenuItem("LuaAnimationNode")) {
            std::string name = "lua_node_" + std::to_string(++counter);
            if (animator) {
                // Create a LuaAnimationNode with a no-op callback
                sol::function noop = mLua.load("return function(node) end")().get<sol::function>();
                auto* node = new LuaAnimationNode(name, noop);
                node->addInput("in");
                node->addOutput("out");
                animator->registerNode(node);
            }
        }
        if (ImGui::MenuItem("AccumulatorNode")) {
            if (animator) {
                auto* node = new AccumulatorNode();
                animator->registerNode(node);
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
                    auto* animator = Animator::instance();
                    if (animator) {
                        auto* from = animator->getRegisteredNode(it->fromNode);
                        auto* to   = animator->getRegisteredNode(it->toNode);
                        if (from && to) {
                            auto& outs = from->getOutputs();
                            auto& ins  = to->getInputs();
                            auto oit = outs.find(it->fromPort);
                            auto iit = ins.find(it->toPort);
                            if (oit != outs.end() && iit != ins.end()) {
                                animator->unlink(oit->second, iit->second);
                            }
                        }
                    }
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
        ImGui::OpenPopup("NodeContextMenu");
    }
    if (ImGui::BeginPopup("NodeContextMenu")) {
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

} // namespace bbfx
