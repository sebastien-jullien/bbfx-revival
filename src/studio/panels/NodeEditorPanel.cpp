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
#include "../../core/ParamSpec.h"

#include <nlohmann/json.hpp>
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

int NodeEditorPanel::sCopyCounter = 0;

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
        nd.typeName = node->getTypeName();
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
        bool anyMatch = false;

        // Collect all nodes belonging to this preset
        std::vector<std::string> presetNodes;
        for (auto& [name, nd] : mNodes) {
            if (name == mDropPresetName || name.rfind(prefix, 0) == 0)
                presetNodes.push_back(name);
        }
        anyMatch = !presetNodes.empty();

        if (anyMatch) {
            // Build adjacency from DAG links between these preset nodes
            auto* animator = Animator::instance();
            std::map<std::string, std::vector<std::string>> children; // parent → [children]
            std::set<std::string> hasParent;
            if (animator) {
                auto links = animator->getLinks();
                std::set<std::string> presetSet(presetNodes.begin(), presetNodes.end());
                for (auto& lk : links) {
                    if (presetSet.count(lk.fromNode) && presetSet.count(lk.toNode)) {
                        // Reverse direction: FX (target) is root, mesh (source) is child
                        // This matches the visual flow: FX on left, mesh on right
                        children[lk.toNode].push_back(lk.fromNode);
                        hasParent.insert(lk.fromNode);
                    }
                }
            }

            // Find roots (nodes with no parent within the preset)
            std::vector<std::string> roots;
            for (auto& n : presetNodes) {
                if (!hasParent.count(n)) roots.push_back(n);
            }
            if (roots.empty()) roots.push_back(presetNodes[0]); // fallback

            // BFS layout: horizontal = depth level, vertical = branch index
            constexpr float kLevelSpacingX = 300.0f;
            constexpr float kBranchSpacingY = 200.0f;
            std::set<std::string> visited;
            std::queue<std::pair<std::string, int>> bfsQueue; // (node, depth)
            std::map<int, int> depthCount; // how many nodes at each depth (for Y offset)

            for (auto& r : roots) {
                bfsQueue.push({r, 0});
                visited.insert(r);
            }

            while (!bfsQueue.empty()) {
                auto [nodeName, depth] = bfsQueue.front();
                bfsQueue.pop();

                int yIdx = depthCount[depth]++;
                float px = canvasPos.x + depth * kLevelSpacingX;
                float py = canvasPos.y + yIdx * kBranchSpacingY;
                mPendingPositions.push_back({nodeName, px, py});

                for (auto& child : children[nodeName]) {
                    if (!visited.count(child)) {
                        visited.insert(child);
                        bfsQueue.push({child, depth + 1});
                    }
                }
            }

            // Position any orphan nodes (not reached by BFS)
            float orphanX = canvasPos.x + static_cast<int>(depthCount.size()) * kLevelSpacingX;
            for (auto& n : presetNodes) {
                if (!visited.count(n)) {
                    mPendingPositions.push_back({n, orphanX, canvasPos.y});
                    orphanX += kLevelSpacingX;
                }
            }
        }
        if (!anyMatch) {
            // Nodes not yet synced — queue with prefix marker for retry
            mPendingPositions.push_back({mDropPresetName, canvasPos.x, canvasPos.y});
        }
        mDropPresetName.clear();
    }

    // Deferred positioning for texture/material asset drops (same pattern)
    if (!mDropAssetNodeName.empty()) {
        auto canvasPos = ned::ScreenToCanvas(mDropAssetScreenPos);
        if (!mDropAssetTarget.empty()) {
            auto tIt = mNodes.find(mDropAssetTarget);
            if (tIt != mNodes.end()) {
                auto tpos = ned::GetNodePosition(tIt->second.id);
                auto tsz = ned::GetNodeSize(tIt->second.id);
                canvasPos.x = tpos.x + tsz.x + 50;
                canvasPos.y = tpos.y;  // align horizontally with target

                // Avoid stacking: shift down iteratively until no overlap
                constexpr float kNodeH = 80.0f;
                bool shifted = true;
                while (shifted) {
                    shifted = false;
                    for (auto& [name, nd] : mNodes) {
                        auto npos = ned::GetNodePosition(nd.id);
                        if (std::abs(npos.x - canvasPos.x) < 50 && std::abs(npos.y - canvasPos.y) < kNodeH) {
                            canvasPos.y = npos.y + kNodeH;
                            shifted = true;
                        }
                    }
                    for (auto& pp : mPendingPositions) {
                        if (std::abs(pp.x - canvasPos.x) < 50 && std::abs(pp.y - canvasPos.y) < kNodeH) {
                            canvasPos.y = pp.y + kNodeH;
                            shifted = true;
                        }
                    }
                }
            }
        }
        mPendingPositions.push_back({mDropAssetNodeName, canvasPos.x, canvasPos.y});
        mDropAssetNodeName.clear();
        mDropAssetTarget.clear();
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

        // Check if node is collapsed — only show connected pins
        bool isCollapsed = mCollapsedNodes.count(name) > 0;
        std::vector<ned::PinId> connectedPins;
        if (isCollapsed) {
            for (auto& lk : mLinks) {
                if (lk.fromNode == name || lk.toNode == name) {
                    connectedPins.push_back(lk.startPin);
                    connectedPins.push_back(lk.endPin);
                }
            }
            // Show collapse indicator
            ImGui::TextDisabled("[collapsed]");
        }

        // Input pins (left)
        for (size_t i = 0; i < nd.inputPins.size(); ++i) {
            if (isCollapsed && std::find(connectedPins.begin(), connectedPins.end(), nd.inputPins[i]) == connectedPins.end())
                continue; // skip unconnected pins when collapsed
            ned::BeginPin(nd.inputPins[i], ned::PinKind::Input);
            ImGui::Text("> %s", nd.inputNames[i].c_str());
            if (!isCollapsed && node) {
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
            if (isCollapsed && std::find(connectedPins.begin(), connectedPins.end(), nd.outputPins[i]) == connectedPins.end())
                continue;
            ned::BeginPin(nd.outputPins[i], ned::PinKind::Output);
            ImGui::Text("%s >", nd.outputNames[i].c_str());
            if (!isCollapsed && node) {
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
    // ── Delete key: delete selected node(s) via CommandManager (undoable) ──
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !mSelectedNodes.empty()) {
        // Filter out non-deletable nodes (RootTimeNode, BeatDetectorNode)
        std::vector<std::string> toDelete;
        for (auto& name : mSelectedNodes) {
            auto it = mNodes.find(name);
            if (it != mNodes.end()) {
                if (it->second.typeName != "RootTimeNode" && it->second.typeName != "BeatDetectorNode")
                    toDelete.push_back(name);
            }
        }

        if (toDelete.size() == 1) {
            CommandManager::instance().execute(
                std::make_unique<DeleteNodeCommand>(toDelete[0], mLua));
        } else if (toDelete.size() > 1) {
            // Confirmation dialog for > 3 nodes
            bool proceed = true;
            if (toDelete.size() > 3) {
                // ImGui popup needs to be deferred — use a flag
                // For simplicity, skip confirmation and always delete (TODO: add modal in polish)
                proceed = true;
            }
            if (proceed) {
                auto compound = std::make_unique<CompoundCommand>(
                    "Delete " + std::to_string(toDelete.size()) + " nodes");
                for (auto& name : toDelete) {
                    compound->add(std::make_unique<DeleteNodeCommand>(name, mLua));
                }
                CommandManager::instance().execute(std::move(compound));
            }
        }
        mSelectedNode.clear();
        mSelectedNodes.clear();
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

    // ── Ctrl+G: create node group from multi-select ────────────────────────
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G) && mSelectedNodes.size() >= 2) {
        static int grpShortcutCounter = 0;
        NodeGroup grp;
        grp.name = "Group " + std::to_string(++grpShortcutCounter);
        grp.hue = static_cast<float>(grpShortcutCounter % 10) / 10.0f;
        grp.memberNames.assign(mSelectedNodes.begin(), mSelectedNodes.end());
        mNodeGroups.push_back(grp);
        std::cout << "[NodeEditor] Created group '" << grp.name << "' with " << grp.memberNames.size() << " nodes" << std::endl;
    }

    // ── Ctrl+C: copy selected nodes + internal links to clipboard ─────────
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && !mSelectedNodes.empty()) {
        auto* anim = Animator::instance();
        mClipboard = {};

        // Compute centroid of selected nodes
        float cx = 0, cy = 0;
        int cnt = 0;
        for (auto& name : mSelectedNodes) {
            auto it = mNodes.find(name);
            if (it != mNodes.end()) {
                auto pos = ned::GetNodePosition(it->second.id);
                cx += pos.x; cy += pos.y; cnt++;
            }
        }
        if (cnt > 0) { cx /= cnt; cy /= cnt; }

        // Serialize each selected node
        for (auto& name : mSelectedNodes) {
            auto it = mNodes.find(name);
            if (it == mNodes.end()) continue;
            NodeSnapshot snap;
            snap.name = name;
            snap.typeName = it->second.typeName;
            auto pos = ned::GetNodePosition(it->second.id);
            snap.relX = pos.x - cx;
            snap.relY = pos.y - cy;

            if (anim) {
                auto* node = anim->getRegisteredNode(name);
                if (node) {
                    for (auto& [pname, port] : node->getInputs())
                        snap.portValues[pname] = port->getValue();
                    if (node->getParamSpec())
                        snap.paramSpecJsonStr = node->getParamSpec()->toJson().dump();
                }
            }
            mClipboard.nodes.push_back(std::move(snap));
        }

        // Serialize internal links (both endpoints in selection)
        if (anim) {
            auto dagLinks = anim->getLinks();
            for (auto& dl : dagLinks) {
                if (mSelectedNodes.count(dl.fromNode) && mSelectedNodes.count(dl.toNode)) {
                    mClipboard.links.push_back({dl.fromNode, dl.fromPort, dl.toNode, dl.toPort});
                }
            }
        }

        std::cout << "[NodeEditor] Copied " << mClipboard.nodes.size()
                  << " nodes, " << mClipboard.links.size() << " links" << std::endl;
    }

    // ── Ctrl+V: paste nodes from clipboard ───────────────────────────────────
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) && !mClipboard.nodes.empty()) {
        auto* anim = Animator::instance();
        if (anim) {
            ++sCopyCounter;
            std::string suffix = "_copy" + std::to_string(sCopyCounter);

            // Build name mapping: original → pasted
            std::map<std::string, std::string> nameMap;
            for (auto& snap : mClipboard.nodes) {
                std::string newName = snap.name + suffix;
                // Ensure unique
                while (anim->getRegisteredNode(newName) || mNodes.count(newName))
                    newName += "_";
                nameMap[snap.name] = newName;
            }

            // Build compound command
            auto compound = std::make_unique<CompoundCommand>(
                "Paste " + std::to_string(mClipboard.nodes.size()) + " nodes");

            // Create nodes
            for (auto& snap : mClipboard.nodes) {
                compound->add(std::make_unique<CreateNodeCommand>(
                    snap.typeName, nameMap[snap.name], mLua));
            }

            // Create internal links (using remapped names)
            for (auto& lk : mClipboard.links) {
                auto fromIt = nameMap.find(lk.fromNode);
                auto toIt = nameMap.find(lk.toNode);
                if (fromIt != nameMap.end() && toIt != nameMap.end()) {
                    compound->add(std::make_unique<CreateLinkCommand>(
                        fromIt->second, lk.fromPort, toIt->second, lk.toPort));
                }
            }

            CommandManager::instance().execute(std::move(compound));

            // Restore port values and ParamSpec after node creation
            for (auto& snap : mClipboard.nodes) {
                auto* newNode = anim->getRegisteredNode(nameMap[snap.name]);
                if (!newNode) continue;

                // Restore ParamSpec
                if (!snap.paramSpecJsonStr.empty() && newNode->getParamSpec()) {
                    try {
                        auto j = nlohmann::json::parse(snap.paramSpecJsonStr);
                        newNode->getParamSpec()->fromJson(j);
                    } catch (...) {}
                }

                // Restore port values
                for (auto& [pname, val] : snap.portValues) {
                    auto& inputs = newNode->getInputs();
                    auto portIt = inputs.find(pname);
                    if (portIt != inputs.end())
                        portIt->second->setValue(val);
                }
            }

            // Position pasted nodes at offset from current view center
            auto viewCenter = ned::ScreenToCanvas(ImVec2(
                ImGui::GetWindowPos().x + ImGui::GetWindowWidth() * 0.5f,
                ImGui::GetWindowPos().y + ImGui::GetWindowHeight() * 0.5f));
            float offsetX = 100.0f * sCopyCounter;
            float offsetY = 50.0f * sCopyCounter;
            for (auto& snap : mClipboard.nodes) {
                mPendingPositions.push_back({
                    nameMap[snap.name],
                    viewCenter.x + snap.relX + offsetX,
                    viewCenter.y + snap.relY + offsetY
                });
            }

            // Select pasted nodes
            mSelectedNodes.clear();
            for (auto& [orig, pasted] : nameMap) {
                mSelectedNodes.insert(pasted);
            }
            mSelectedNode = mSelectedNodes.empty() ? "" : *mSelectedNodes.begin();

            std::cout << "[NodeEditor] Pasted " << nameMap.size() << " nodes" << std::endl;
        }
    }

    // ── Quick-add popup: double-click in void or Ctrl+Space ────────────────
    if ((ImGui::IsMouseDoubleClicked(0) && !ned::GetHoveredNode() && !ned::GetHoveredLink() && !ned::GetHoveredPin()) ||
        (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space))) {
        mShowQuickAdd = true;
        mQuickAddPos = ned::ScreenToCanvas(ImGui::GetMousePos());
        std::memset(mQuickAddBuf, 0, sizeof(mQuickAddBuf));
        mQuickAddSelection = 0;
    }

    // ── Ctrl+L: Smart wire ───────────────────────────────────────────────────
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L) && mSelectedNodes.size() == 2) {
        auto it = mSelectedNodes.begin();
        std::string node1 = *it++;
        std::string node2 = *it;
        auto* anim = Animator::instance();
        if (anim) {
            auto* n1 = anim->getRegisteredNode(node1);
            auto* n2 = anim->getRegisteredNode(node2);
            if (n1 && n2) {
                // Try to match output of n1 → input of n2
                bool linked = false;
                for (auto& [outName, outPort] : n1->getOutputs()) {
                    for (auto& [inName, inPort] : n2->getInputs()) {
                        if (outName == inName || (outName == "entity" && inName == "entity") ||
                            (outName == "out" && (inName == "in" || inName == "amplitude"))) {
                            CommandManager::instance().execute(
                                std::make_unique<CreateLinkCommand>(node1, outName, node2, inName));
                            linked = true;
                            break;
                        }
                    }
                    if (linked) break;
                }
                if (!linked) {
                    // Try reverse: output of n2 → input of n1
                    for (auto& [outName, outPort] : n2->getOutputs()) {
                        for (auto& [inName, inPort] : n1->getInputs()) {
                            if (outName == inName || (outName == "entity" && inName == "entity") ||
                                (outName == "out" && (inName == "in" || inName == "amplitude"))) {
                                CommandManager::instance().execute(
                                    std::make_unique<CreateLinkCommand>(node2, outName, node1, inName));
                                linked = true;
                                break;
                            }
                        }
                        if (linked) break;
                    }
                }
                if (!linked)
                    std::cout << "[NodeEditor] Smart wire: no compatible ports found" << std::endl;
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
        int selCount = ned::GetSelectedObjectCount();
        std::vector<ned::NodeId> selectedIds(selCount > 0 ? selCount : 1);
        int actualCount = ned::GetSelectedNodes(selectedIds.data(), static_cast<int>(selectedIds.size()));

        std::set<std::string> newSelection;
        for (int i = 0; i < actualCount; ++i) {
            for (auto& [name, nd] : mNodes) {
                if (nd.id == selectedIds[i]) {
                    newSelection.insert(name);
                    break;
                }
            }
        }

        if (newSelection != mSelectedNodes) {
            mSelectedNodes = newSelection;

            // Derive primary selection (first alphabetically, or empty)
            std::string newPrimary = mSelectedNodes.empty() ? "" : *mSelectedNodes.begin();
            mSelectedNode = newPrimary;

            // Always fire callback when selection set changes — Inspector needs the full set
            if (mSelectionCallback) mSelectionCallback(mSelectedNode);

            if (mSelectedNodes.size() > 1) {
                std::cout << "[NodeEditor] Selected " << mSelectedNodes.size() << " nodes" << std::endl;
            }
        }
    }

    // Cache view rect for minimap (must be inside Begin/End scope)
    {
        auto* edCtx2 = reinterpret_cast<ax::NodeEditor::Detail::EditorContext*>(mEditorContext);
        auto vr = edCtx2->GetViewRect();
        mViewRect[0] = vr.Min.x; mViewRect[1] = vr.Min.y;
        mViewRect[2] = vr.Max.x; mViewRect[3] = vr.Max.y;
    }

    // Cache node rects + mouse canvas position for hit-test during drag-drop
    // (drag-drop target is processed AFTER ned::End, so we can't call ned:: API there)
    mCachedNodeRects.clear();
    for (auto& [name, nd] : mNodes) {
        auto npos = ned::GetNodePosition(nd.id);
        auto nsz = ned::GetNodeSize(nd.id);
        mCachedNodeRects[name] = {npos.x, npos.y, nsz.x, nsz.y};
    }
    // Cache screen→canvas transformation for use after ned::End
    // Store two reference points to compute the linear transform
    mCanvasRef0 = ned::ScreenToCanvas({0, 0});
    mCanvasRef1 = ned::ScreenToCanvas({1, 0});

    ned::End();
    ned::SetCurrentEditor(nullptr);

    // Apply pending positions AFTER ned::End — SetNodePosition works outside Begin/End scope
    if (!mPendingPositions.empty()) {
        ned::SetCurrentEditor(mEditorContext);
        std::vector<NodePosition> remaining;
        for (auto& np : mPendingPositions) {
            auto it = mNodes.find(np.name);
            if (it != mNodes.end()) {
                ned::SetNodePosition(it->second.id, {np.x, np.y});
                np.attempts++;
                if (np.attempts < 3) remaining.push_back(np);
            } else {
                std::string prefix = np.name + "_";
                int nodeIdx = 0;
                bool expanded = false;
                for (auto& [name, nd] : mNodes) {
                    if (name.rfind(prefix, 0) == 0) {
                        float px = np.x + nodeIdx * 350.0f;
                        float py = np.y + (nodeIdx % 2) * 100.0f;
                        ned::SetNodePosition(nd.id, {px, py});
                        nodeIdx++;
                        expanded = true;
                    }
                }
                if (!expanded) remaining.push_back(np);
                else { np.attempts++; if (np.attempts < 3) remaining.push_back(np); }
            }
        }
        mPendingPositions = std::move(remaining);
        ned::SetCurrentEditor(nullptr);
    }

    // ── Node groups: visual rendering only (interactions in showNodeContextMenu via ned::Suspend) ──
    // Clean dead members from groups (BEFORE rendering to avoid iterator invalidation)
    for (size_t gi = 0; gi < mNodeGroups.size(); ++gi) {
        auto& grp = mNodeGroups[gi];
        grp.memberNames.erase(
            std::remove_if(grp.memberNames.begin(), grp.memberNames.end(),
                [this](const std::string& n) { return mNodes.find(n) == mNodes.end(); }),
            grp.memberNames.end());
        if (grp.memberNames.empty()) {
            mNodeGroups.erase(mNodeGroups.begin() + gi);
            --gi;
        }
    }

    if (!mNodeGroups.empty()) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float cScale = mCanvasRef1.x - mCanvasRef0.x;
        if (std::abs(cScale) > 0.0001f) {
            float invScale = 1.0f / cScale;
            auto canvasToScreen = [&](float cx, float cy) -> ImVec2 {
                return {(cx - mCanvasRef0.x) * invScale, (cy - mCanvasRef0.y) * invScale};
            };

            for (auto& grp : mNodeGroups) {
                float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
                for (auto& name : grp.memberNames) {
                    auto it = mCachedNodeRects.find(name);
                    if (it == mCachedNodeRects.end()) continue;
                    minX = std::min(minX, it->second.x);
                    minY = std::min(minY, it->second.y);
                    maxX = std::max(maxX, it->second.x + it->second.w);
                    maxY = std::max(maxY, it->second.y + it->second.h);
                }
                if (minX >= FLT_MAX) continue;

                float pad = 20.0f;
                ImVec2 p1 = canvasToScreen(minX - pad, minY - pad - 22);
                ImVec2 p2 = canvasToScreen(maxX + pad, maxY + pad);

                // Body
                ImVec4 col4 = ImColor::HSV(grp.hue, 0.3f, 0.4f, 0.15f);
                ImVec4 border4 = ImColor::HSV(grp.hue, 0.5f, 0.6f, 0.5f);
                dl->AddRectFilled(p1, p2, ImGui::ColorConvertFloat4ToU32(col4), 8.0f);
                dl->AddRect(p1, p2, ImGui::ColorConvertFloat4ToU32(border4), 8.0f, 0, 1.5f);

                // Title bar
                ImVec2 titleP2 = {p2.x, p1.y + 20};
                ImVec4 titleBg = ImColor::HSV(grp.hue, 0.4f, 0.35f, 0.5f);
                dl->AddRectFilled(p1, titleP2, ImGui::ColorConvertFloat4ToU32(titleBg), 8.0f, ImDrawFlags_RoundCornersTop);
                ImVec4 textCol = ImColor::HSV(grp.hue, 0.3f, 0.95f, 1.0f);
                dl->AddText({p1.x + 8, p1.y + 3}, ImGui::ColorConvertFloat4ToU32(textCol), grp.name.c_str());
                std::string countStr = "(" + std::to_string(grp.memberNames.size()) + ")";
                ImVec2 countSz = ImGui::CalcTextSize(countStr.c_str());
                dl->AddText({titleP2.x - countSz.x - 8, p1.y + 3}, IM_COL32(180, 180, 180, 180), countStr.c_str());
            }
        }
    }

    // ── Node comments overlay (yellow bubble above node) ─────────────────
    if (!mNodeComments.empty()) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        // Canvas→screen conversion using cached refs
        // mCanvasRef0 = ScreenToCanvas({0,0}), mCanvasRef1 = ScreenToCanvas({1,0})
        // scale = mCanvasRef1.x - mCanvasRef0.x (canvas units per screen pixel)
        float cScale = mCanvasRef1.x - mCanvasRef0.x;
        if (std::abs(cScale) > 0.0001f) {
            float invScale = 1.0f / cScale;
            auto canvasToScreen = [&](float cx, float cy) -> ImVec2 {
                return {(cx - mCanvasRef0.x) * invScale, (cy - mCanvasRef0.y) * invScale};
            };

            for (auto& [name, comment] : mNodeComments) {
                auto it = mCachedNodeRects.find(name);
                if (it == mCachedNodeRects.end()) continue;
                auto& rect = it->second;
                ImVec2 screenPos = canvasToScreen(rect.x, rect.y);
                // Position bubble above the node
                float bubbleY = screenPos.y - 30;
                float bubbleX = screenPos.x;

                // Truncate to first 50 chars
                std::string display = comment.substr(0, 50);
                if (comment.size() > 50) display += "...";

                ImVec2 textSz = ImGui::CalcTextSize(display.c_str());
                float pad = 4;
                ImVec2 p1 = {bubbleX - pad, bubbleY - pad};
                ImVec2 p2 = {bubbleX + textSz.x + pad, bubbleY + textSz.y + pad};

                dl->AddRectFilled(p1, p2, IM_COL32(200, 180, 50, 200), 4.0f);
                dl->AddRect(p1, p2, IM_COL32(180, 160, 30, 255), 4.0f);
                dl->AddText({bubbleX, bubbleY}, IM_COL32(0, 0, 0, 255), display.c_str());
            }
        }
    }

    // ── Minimap (interactive overview in bottom-right corner) ──────────────
    if (!mNodes.empty() && mCachedNodeRects.size() >= 2) {
        // 1. Compute global bounds of ALL nodes (canvas coords)
        float gMinX = FLT_MAX, gMinY = FLT_MAX, gMaxX = -FLT_MAX, gMaxY = -FLT_MAX;
        for (auto& [name, rect] : mCachedNodeRects) {
            gMinX = std::min(gMinX, rect.x);
            gMinY = std::min(gMinY, rect.y);
            gMaxX = std::max(gMaxX, rect.x + rect.w);
            gMaxY = std::max(gMaxY, rect.y + rect.h);
        }
        if (gMinX >= FLT_MAX) goto skipMinimap;

        // 2. Check if the entire graph fits in the current view — if yes, hide minimap
        {
            float viewW = mViewRect[2] - mViewRect[0];
            float viewH = mViewRect[3] - mViewRect[1];
            float graphW = gMaxX - gMinX;
            float graphH = gMaxY - gMinY;
            if (graphW < viewW * 0.9f && graphH < viewH * 0.9f &&
                gMinX > mViewRect[0] && gMaxX < mViewRect[2] &&
                gMinY > mViewRect[1] && gMaxY < mViewRect[3]) {
                goto skipMinimap; // entire graph visible, no minimap needed
            }
        }

        {
            ImVec2 parentWinPos = ImGui::GetWindowPos();
            ImVec2 parentWinSize = ImGui::GetWindowSize();
            float mmW = 160, mmH = 110;
            float mmPad = 8.0f;
            ImVec2 mmPos = {parentWinPos.x + parentWinSize.x - mmW - 10,
                            parentWinPos.y + parentWinSize.y - mmH - 10};

            // Use a separate floating window so it captures mouse independently of node editor
            ImGui::SetNextWindowPos(mmPos);
            ImGui::SetNextWindowSize({mmW, mmH});
            ImGui::SetNextWindowBgAlpha(0.85f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
            ImGui::Begin("##Minimap", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoDocking);

            // Recompute mmPos from the actual window position (may differ slightly)
            mmPos = ImGui::GetWindowPos();
            ImVec2 mmSz = ImGui::GetWindowSize();
            mmW = mmSz.x; mmH = mmSz.y;

            // World bounds = union of all nodes + current view
            float worldMinX = std::min(gMinX, mViewRect[0]);
            float worldMinY = std::min(gMinY, mViewRect[1]);
            float worldMaxX = std::max(gMaxX, mViewRect[2]);
            float worldMaxY = std::max(gMaxY, mViewRect[3]);
            float worldW = std::max(1.0f, worldMaxX - worldMinX);
            float worldH = std::max(1.0f, worldMaxY - worldMinY);

            float scaleX = (mmW - 2 * mmPad) / worldW;
            float scaleY = (mmH - 2 * mmPad) / worldH;
            float scale = std::min(scaleX, scaleY);

            float contentW = worldW * scale;
            float contentH = worldH * scale;
            float offsetX = mmPos.x + mmPad + (mmW - 2 * mmPad - contentW) * 0.5f;
            float offsetY = mmPos.y + mmPad + (mmH - 2 * mmPad - contentH) * 0.5f;

            auto canvasToMM = [&](float cx, float cy) -> ImVec2 {
                return {offsetX + (cx - worldMinX) * scale, offsetY + (cy - worldMinY) * scale};
            };

            ImDrawList* dl = ImGui::GetWindowDrawList();

            // Draw links
            for (auto& lk : mLinks) {
                auto fromIt = mCachedNodeRects.find(lk.fromNode);
                auto toIt = mCachedNodeRects.find(lk.toNode);
                if (fromIt != mCachedNodeRects.end() && toIt != mCachedNodeRects.end()) {
                    auto& fr = fromIt->second;
                    auto& tr = toIt->second;
                    ImVec2 p1 = canvasToMM(fr.x + fr.w, fr.y + fr.h * 0.5f);
                    ImVec2 p2 = canvasToMM(tr.x, tr.y + tr.h * 0.5f);
                    dl->AddLine(p1, p2, IM_COL32(255, 160, 50, 80), 1.0f);
                }
            }

            // Draw nodes
            for (auto& [name, rect] : mCachedNodeRects) {
                ImVec2 p1 = canvasToMM(rect.x, rect.y);
                ImVec2 p2 = canvasToMM(rect.x + rect.w, rect.y + rect.h);
                if (p2.x - p1.x < 6) p2.x = p1.x + 6;
                if (p2.y - p1.y < 4) p2.y = p1.y + 4;

                auto nodeIt = mNodes.find(name);
                ImVec4 col4 = nodeIt != mNodes.end() ? nodeColor(nodeIt->second.typeName) : ImVec4(0.8f,0.8f,0.8f,1);
                ImU32 fillCol = ImGui::ColorConvertFloat4ToU32({col4.x, col4.y, col4.z, 0.7f});
                ImU32 borderCol = ImGui::ColorConvertFloat4ToU32({col4.x, col4.y, col4.z, 1.0f});
                if (mSelectedNodes.count(name) > 0) {
                    fillCol = ImGui::ColorConvertFloat4ToU32({col4.x, col4.y, col4.z, 1.0f});
                    borderCol = IM_COL32(255, 255, 100, 255);
                }
                dl->AddRectFilled(p1, p2, fillCol);
                dl->AddRect(p1, p2, borderCol);
            }

            // Draw viewport rectangle
            {
                ImVec2 vp1 = canvasToMM(mViewRect[0], mViewRect[1]);
                ImVec2 vp2 = canvasToMM(mViewRect[2], mViewRect[3]);
                dl->AddRect(vp1, vp2, IM_COL32(255, 255, 255, 180), 0.0f, 0, 1.5f);
                dl->AddRectFilled(vp1, vp2, IM_COL32(255, 255, 255, 15));
            }

            // Interactive: click/drag in minimap window = navigate canvas
            // The window itself captures mouse, preventing node editor box-select
            if (ImGui::IsWindowHovered() && (ImGui::IsMouseClicked(0) || ImGui::IsMouseDragging(0))) {
                ImVec2 mousePos = ImGui::GetMousePos();
                float canvasX = worldMinX + (mousePos.x - offsetX) / scale;
                float canvasY = worldMinY + (mousePos.y - offsetY) / scale;
                float viewW = mViewRect[2] - mViewRect[0];
                float viewH = mViewRect[3] - mViewRect[1];
                ImRect target({canvasX - viewW * 0.5f, canvasY - viewH * 0.5f},
                              {canvasX + viewW * 0.5f, canvasY + viewH * 0.5f});
                ned::SetCurrentEditor(mEditorContext);
                auto* edCtxMM = reinterpret_cast<ax::NodeEditor::Detail::EditorContext*>(mEditorContext);
                edCtxMM->NavigateTo(target, false, -1.0f);
                ned::SetCurrentEditor(nullptr);
            }

            ImGui::End(); // ##Minimap
            ImGui::PopStyleVar(2);
        }
    }
    skipMinimap:;

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
    // Use window-level drop target (works even after ned::End)
    if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->ContentRegionRect,
                                          ImGui::GetCurrentWindow()->ID)) {
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
        // Accept particle drag → create ParticleNode
        if (auto* payload = ImGui::AcceptDragDropPayload("PARTICLE_NAME")) {
            std::string particleName(static_cast<const char*>(payload->Data));
            mDropScreenPos = ImGui::GetMousePos();
            sol::optional<sol::table> dbg = mLua["dbg"];
            if (dbg) {
                sol::optional<sol::function> createFn = (*dbg)["create"];
                if (createFn) (*createFn)("ParticleNode", particleName);
            }
        }
        // Accept compositor drag → create CompositorNode with correct compositor name
        if (auto* payload = ImGui::AcceptDragDropPayload("COMPOSITOR_NAME")) {
            std::string compName(static_cast<const char*>(payload->Data));
            mDropScreenPos = ImGui::GetMousePos();
            // Generate a clean node name from the compositor name
            std::string nodeName = "comp_" + compName;
            // Replace dots/spaces with underscores for valid node name
            for (auto& ch : nodeName) { if (ch == '.' || ch == ' ') ch = '_'; }
            sol::optional<sol::table> dbg = mLua["dbg"];
            if (dbg) {
                sol::optional<sol::function> cwpFn = (*dbg)["create_with_param"];
                if (cwpFn) (*cwpFn)("CompositorNode", nodeName, "compositor", compName);
            }
        }
        // Accept shader drag → create ShaderFxNode with the dropped shader
        if (auto* payload = ImGui::AcceptDragDropPayload("SHADER_NAME")) {
            std::string shaderName(static_cast<const char*>(payload->Data));
            mDropScreenPos = ImGui::GetMousePos();
            // Determine vert/frag paths by extension
            bool isFrag = (shaderName.find(".frag") != std::string::npos);
            std::string vertShader = isFrag ? "passthrough.vert" : shaderName;
            std::string fragShader = isFrag ? shaderName : "passthrough.frag";
            // Generate a clean node name
            std::string nodeName = "shader_" + shaderName;
            for (auto& ch : nodeName) { if (ch == '.' || ch == ' ') ch = '_'; }
            // Use deferred creation with shader paths (injected into _preset_params at creation time)
            sol::optional<sol::table> dbg = mLua["dbg"];
            if (dbg) {
                sol::optional<sol::function> cwsFn = (*dbg)["create_with_shader"];
                if (cwsFn) (*cwsFn)(nodeName, vertShader, fragShader);
            }
        }
        // Helper: find SceneObjectNode under cursor using cached transform + node rects
        auto findSceneObjUnderCursor = [this]() -> std::string {
            auto* anim = Animator::instance();
            if (!anim) return "";
            // Convert drop screen position to canvas coords using cached transform
            auto canvasPos = screenToCanvasCached(mDropScreenPos);
            for (auto& [name, rect] : mCachedNodeRects) {
                if (canvasPos.x >= rect.x && canvasPos.x <= rect.x + rect.w &&
                    canvasPos.y >= rect.y && canvasPos.y <= rect.y + rect.h) {
                    auto* node = anim->getRegisteredNode(name);
                    if (node && node->getTypeName() == "SceneObjectNode")
                        return name;
                }
            }
            return "";
        };

        // Accept texture → create TextureNode + auto-link to SceneObjectNode under cursor
        if (auto* payload = ImGui::AcceptDragDropPayload("TEXTURE_NAME")) {
            std::string texName(static_cast<const char*>(payload->Data));
            mDropScreenPos = ImGui::GetMousePos();
            // Hit-test: find SceneObjectNode under cursor via cached rects
            std::string linkTarget = findSceneObjUnderCursor();
            // Fallback: use selected node
            if (linkTarget.empty() && !mSelectedNode.empty()) {
                auto* anim = Animator::instance();
                auto* selNode = anim ? anim->getRegisteredNode(mSelectedNode) : nullptr;
                if (selNode && selNode->getTypeName() == "SceneObjectNode")
                    linkTarget = mSelectedNode;
            }
            if (mCreateNodeFn) {
                mCreateNodeFn("TextureNode", texName, linkTarget);
            }
        }
        // Accept material → create MaterialNode + auto-link to SceneObjectNode under cursor
        if (auto* payload = ImGui::AcceptDragDropPayload("MATERIAL_NAME")) {
            std::string matName(static_cast<const char*>(payload->Data));
            mDropScreenPos = ImGui::GetMousePos();
            std::string linkTarget = findSceneObjUnderCursor();
            if (linkTarget.empty() && !mSelectedNode.empty()) {
                auto* anim = Animator::instance();
                auto* selNode = anim ? anim->getRegisteredNode(mSelectedNode) : nullptr;
                if (selNode && selNode->getTypeName() == "SceneObjectNode")
                    linkTarget = mSelectedNode;
            }
            if (mCreateNodeFn) {
                mCreateNodeFn("MaterialNode", matName, linkTarget);
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

    // Detect drag-link to void (QueryNewNode fires when link is dropped on empty canvas)
    ned::PinId newNodePinId;
    if (ned::QueryNewNode(&newNodePinId)) {
        if (ned::AcceptNewItem()) {
            // Find which node/port this pin belongs to
            for (auto& [name, nd] : mNodes) {
                for (size_t i = 0; i < nd.outputPins.size(); ++i) {
                    if (nd.outputPins[i] == newNodePinId) {
                        mPendingDragLink = {name, nd.outputNames[i], true, true};
                        mShowQuickAdd = true;
                        mQuickAddPos = ned::ScreenToCanvas(ImGui::GetMousePos());
                        std::memset(mQuickAddBuf, 0, sizeof(mQuickAddBuf));
                        mQuickAddSelection = 0;
                        goto endCreate;
                    }
                }
                for (size_t i = 0; i < nd.inputPins.size(); ++i) {
                    if (nd.inputPins[i] == newNodePinId) {
                        mPendingDragLink = {name, nd.inputNames[i], false, true};
                        mShowQuickAdd = true;
                        mQuickAddPos = ned::ScreenToCanvas(ImGui::GetMousePos());
                        std::memset(mQuickAddBuf, 0, sizeof(mQuickAddBuf));
                        mQuickAddSelection = 0;
                        goto endCreate;
                    }
                }
            }
        }
    }
    endCreate:
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

    // Static state for group context menu
    static int sGrpCtxIdx = -1;
    static char sGrpRenameBuf[128] = {};

    if (ned::ShowBackgroundContextMenu()) {
        mCreateMenuPos = ImGui::GetMousePos();
        // Check if right-click is on a group title bar
        sGrpCtxIdx = -1;
        float cScale = mCanvasRef1.x - mCanvasRef0.x;
        if (std::abs(cScale) > 0.0001f) {
            float invScale = 1.0f / cScale;
            for (int gi = 0; gi < static_cast<int>(mNodeGroups.size()); ++gi) {
                auto& grp = mNodeGroups[gi];
                float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
                for (auto& name : grp.memberNames) {
                    auto it = mCachedNodeRects.find(name);
                    if (it == mCachedNodeRects.end()) continue;
                    minX = std::min(minX, it->second.x);
                    minY = std::min(minY, it->second.y);
                    maxX = std::max(maxX, it->second.x + it->second.w);
                    maxY = std::max(maxY, it->second.y + it->second.h);
                }
                if (minX >= FLT_MAX) continue;
                float pad = 20.0f;
                // Convert group bounds to screen coords
                ImVec2 gp1 = {(minX - pad - mCanvasRef0.x) * invScale,
                              (minY - pad - 22 - mCanvasRef0.y) * invScale};
                ImVec2 gp2 = {(maxX + pad - mCanvasRef0.x) * invScale,
                              (maxY + pad - mCanvasRef0.y) * invScale};
                if (mCreateMenuPos.x >= gp1.x && mCreateMenuPos.x <= gp2.x &&
                    mCreateMenuPos.y >= gp1.y && mCreateMenuPos.y <= gp2.y) {
                    sGrpCtxIdx = gi;
                    std::strncpy(sGrpRenameBuf, grp.name.c_str(), sizeof(sGrpRenameBuf) - 1);
                    break;
                }
            }
        }
        if (sGrpCtxIdx >= 0) {
            ImGui::OpenPopup("GroupContextMenu");
        } else {
            ImGui::OpenPopup("NodeCreationMenu");
        }
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
        // Find which node was right-clicked
        std::string contextName;
        for (auto& [name, nd] : mNodes) {
            if (nd.id == contextNodeId) { contextName = name; break; }
        }
        // If the right-clicked node is already in the multi-selection, keep the selection intact.
        // Otherwise (no selection or node not in selection), select just this node.
        if (!contextName.empty() && mSelectedNodes.find(contextName) == mSelectedNodes.end()) {
            mSelectedNode = contextName;
            mSelectedNodes = {contextName};
            ned::SelectNode(contextNodeId);
            if (mSelectionCallback) mSelectionCallback(contextName);
        }
        // Ensure primary is set for context menu operations
        if (!contextName.empty() && mSelectedNodes.size() <= 1) {
            mSelectedNode = contextName;
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
        // Detach Particle (v3.2.4) — only for ParticleNodes with entity links
        if (!mSelectedNode.empty()) {
            auto* animator2 = Animator::instance();
            auto* selNode = animator2 ? animator2->getRegisteredNode(mSelectedNode) : nullptr;
            if (selNode && selNode->getTypeName() == "ParticleNode") {
                auto& inputs = selNode->getInputs();
                auto entityIt = inputs.find("entity");
                if (entityIt != inputs.end()) {
                    auto* entityPort = entityIt->second;
                    auto sourceNodes = animator2->getSourceNodes(entityPort);
                    if (!sourceNodes.empty()) {
                        if (ImGui::MenuItem("Detach Particle")) {
                            // Find and unlink all source entity ports connected to this particle's entity port
                            for (auto* srcNode : sourceNodes) {
                                auto& srcOuts = srcNode->getOutputs();
                                auto srcEntityIt = srcOuts.find("entity");
                                if (srcEntityIt != srcOuts.end()) {
                                    animator2->unlink(srcEntityIt->second, entityPort);
                                }
                            }
                        }
                    }
                }
            }
        }
        // Node comment
        if (!mSelectedNode.empty()) {
            bool hasComment = mNodeComments.count(mSelectedNode) > 0;
            if (ImGui::MenuItem(hasComment ? "Edit Comment" : "Add Comment")) {
                mShowCommentPopup = true;
                mCommentTarget = mSelectedNode;
                std::memset(mCommentBuf, 0, sizeof(mCommentBuf));
                if (hasComment) {
                    std::strncpy(mCommentBuf, mNodeComments[mSelectedNode].c_str(), sizeof(mCommentBuf) - 1);
                }
            }
            if (hasComment && ImGui::MenuItem("Remove Comment")) {
                mNodeComments.erase(mSelectedNode);
            }
        }

        // Node group (Ctrl+G)
        if (mSelectedNodes.size() >= 2) {
            if (ImGui::MenuItem("Group (Ctrl+G)")) {
                static int grpCounter = 0;
                NodeGroup grp;
                grp.name = "Group " + std::to_string(++grpCounter);
                grp.hue = static_cast<float>(grpCounter % 10) / 10.0f;
                grp.memberNames.assign(mSelectedNodes.begin(), mSelectedNodes.end());
                mNodeGroups.push_back(grp);
            }
        }

        // Remove from group (if node belongs to one)
        if (!mSelectedNode.empty()) {
            for (int gi = 0; gi < static_cast<int>(mNodeGroups.size()); ++gi) {
                auto& grp = mNodeGroups[gi];
                auto it = std::find(grp.memberNames.begin(), grp.memberNames.end(), mSelectedNode);
                if (it != grp.memberNames.end()) {
                    std::string menuLabel = "Remove from \"" + grp.name + "\"";
                    if (ImGui::MenuItem(menuLabel.c_str())) {
                        grp.memberNames.erase(it);
                    }
                    break; // a node can only be in one group
                }
            }
        }

        // Collapse/Expand toggle
        if (!mSelectedNode.empty()) {
            bool collapsed = mCollapsedNodes.count(mSelectedNode) > 0;
            if (ImGui::MenuItem(collapsed ? "Expand" : "Collapse")) {
                if (collapsed) mCollapsedNodes.erase(mSelectedNode);
                else mCollapsedNodes.insert(mSelectedNode);
            }
        }

        // Align / Distribute — uses mCachedNodeRects (safe) + mPendingPositions (deferred apply)
        if (mSelectedNodes.size() >= 2 && ImGui::BeginMenu("Align")) {
            // Helper: get cached position + size for each selected node
            struct NodePosSize { std::string name; float x, y, w, h; };
            auto getCached = [this]() {
                std::vector<NodePosSize> result;
                for (auto& sn : mSelectedNodes) {
                    auto it = mCachedNodeRects.find(sn);
                    if (it != mCachedNodeRects.end())
                        result.push_back({sn, it->second.x, it->second.y, it->second.w, it->second.h});
                }
                return result;
            };
            if (ImGui::MenuItem("Top")) {
                auto nodes = getCached();
                float minY = FLT_MAX;
                for (auto& nd : nodes) minY = std::min(minY, nd.y);
                for (auto& nd : nodes)
                    mPendingPositions.push_back({nd.name, nd.x, minY});
            }
            if (ImGui::MenuItem("Bottom")) {
                auto nodes = getCached();
                float maxBottom = -FLT_MAX;
                for (auto& nd : nodes) maxBottom = std::max(maxBottom, nd.y + nd.h);
                for (auto& nd : nodes)
                    mPendingPositions.push_back({nd.name, nd.x, maxBottom - nd.h});
            }
            if (ImGui::MenuItem("Left")) {
                auto nodes = getCached();
                float minX = FLT_MAX;
                for (auto& nd : nodes) minX = std::min(minX, nd.x);
                for (auto& nd : nodes)
                    mPendingPositions.push_back({nd.name, minX, nd.y});
            }
            if (ImGui::MenuItem("Right")) {
                auto nodes = getCached();
                float maxRight = -FLT_MAX;
                for (auto& nd : nodes) maxRight = std::max(maxRight, nd.x + nd.w);
                for (auto& nd : nodes)
                    mPendingPositions.push_back({nd.name, maxRight - nd.w, nd.y});
            }
            ImGui::EndMenu();
        }
        if (mSelectedNodes.size() >= 2 && ImGui::BeginMenu("Distribute")) {
            if (ImGui::MenuItem("Horizontally (row)")) {
                // Arrange all nodes in a horizontal row: same Y, sequential X with 30px gap
                struct NPS { std::string name; float x, y, w, h; };
                std::vector<NPS> nodes;
                float avgY = 0;
                for (auto& sn : mSelectedNodes) {
                    auto it = mCachedNodeRects.find(sn);
                    if (it != mCachedNodeRects.end()) {
                        nodes.push_back({sn, it->second.x, it->second.y, it->second.w, it->second.h});
                        avgY += it->second.y;
                    }
                }
                if (!nodes.empty()) {
                    avgY /= nodes.size();
                    std::sort(nodes.begin(), nodes.end(), [](auto& a, auto& b) { return a.x < b.x; });
                    constexpr float gap = 30.0f;
                    float curX = nodes.front().x;
                    for (size_t i = 0; i < nodes.size(); ++i) {
                        mPendingPositions.push_back({nodes[i].name, curX, avgY});
                        curX += nodes[i].w + gap;
                    }
                }
            }
            if (ImGui::MenuItem("Vertically (column)")) {
                // Arrange all nodes in a vertical column: same X, sequential Y with 20px gap
                struct NPS { std::string name; float x, y, w, h; };
                std::vector<NPS> nodes;
                float avgX = 0;
                for (auto& sn : mSelectedNodes) {
                    auto it = mCachedNodeRects.find(sn);
                    if (it != mCachedNodeRects.end()) {
                        nodes.push_back({sn, it->second.x, it->second.y, it->second.w, it->second.h});
                        avgX += it->second.x;
                    }
                }
                if (!nodes.empty()) {
                    avgX /= nodes.size();
                    std::sort(nodes.begin(), nodes.end(), [](auto& a, auto& b) { return a.y < b.y; });
                    constexpr float gap = 20.0f;
                    float curY = nodes.front().y;
                    for (size_t i = 0; i < nodes.size(); ++i) {
                        mPendingPositions.push_back({nodes[i].name, avgX, curY});
                        curY += nodes[i].h + gap;
                    }
                }
            }
            ImGui::EndMenu();
        }

        // Batch apply FX — only when multi-select contains SceneObjectNodes
        if (mSelectedNodes.size() >= 2) {
            auto* anim3 = Animator::instance();
            std::vector<std::string> sceneObjs;
            for (auto& sn : mSelectedNodes) {
                auto* n = anim3 ? anim3->getRegisteredNode(sn) : nullptr;
                if (n && n->getTypeName() == "SceneObjectNode")
                    sceneObjs.push_back(sn);
            }
            if (sceneObjs.size() >= 2 && ImGui::BeginMenu("Apply FX...")) {
                static int fxCounter = 0;
                auto applyFx = [&](const std::string& typeName, const std::string& name) {
                    auto compound = std::make_unique<CompoundCommand>("Batch Apply " + typeName);
                    compound->add(std::make_unique<CreateNodeCommand>(typeName, name, mLua));
                    for (auto& target : sceneObjs) {
                        compound->add(std::make_unique<CreateLinkCommand>(
                            target, "entity", name, "entity"));
                    }
                    CommandManager::instance().execute(std::move(compound));
                };
                if (ImGui::MenuItem("PerlinFxNode")) {
                    applyFx("PerlinFxNode", "BatchPerlin_" + std::to_string(++fxCounter));
                }
                if (ImGui::MenuItem("WaveVertexShader")) {
                    applyFx("WaveVertexShader", "BatchWave_" + std::to_string(++fxCounter));
                }
                ImGui::EndMenu();
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Save as Preset")) {
            mShowSavePresetDialog = true;
            std::memset(mPresetNameBuf, 0, sizeof(mPresetNameBuf));
            if (!mSelectedNode.empty()) {
                std::strncpy(mPresetNameBuf, mSelectedNode.c_str(), sizeof(mPresetNameBuf) - 1);
            }
        }
        ImGui::EndPopup();
    }

    // ── Quick-add popup rendering ──────────────────────────────────────────
    if (mShowQuickAdd) {
        ImGui::OpenPopup("QuickAddPopup");
        mShowQuickAdd = false;
    }
    if (ImGui::BeginPopup("QuickAddPopup")) {
        ImGui::SetNextItemWidth(250.0f);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputText("##quickadd", mQuickAddBuf, sizeof(mQuickAddBuf));

        std::string filter(mQuickAddBuf);
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

        // Build filtered list from node types
        auto byCategory = NodeTypeRegistry::instance().getByCategory();
        static int qaCounter = 0;
        int idx = 0;
        for (auto& [cat, types] : byCategory) {
            for (auto* info : types) {
                std::string lower = info->typeName;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (!filter.empty() && lower.find(filter) == std::string::npos) continue;

                bool selected = (idx == mQuickAddSelection);
                std::string label = info->typeName + " (" + cat + ")";
                if (ImGui::Selectable(label.c_str(), selected)) {
                    std::string name = info->typeName + "_" + std::to_string(++qaCounter);
                    if (mPendingDragLink.active) {
                        // Create node + auto-link from/to the dragged pin
                        auto compound = std::make_unique<CompoundCommand>("Quick create + link");
                        compound->add(std::make_unique<CreateNodeCommand>(info->typeName, name, mLua));
                        // Determine which port of the new node to connect to
                        // Simple heuristic: if drag from output, connect to first input; vice versa
                        if (mPendingDragLink.isOutput) {
                            // Dragged from output → connect to first input of new node
                            std::string targetPort = "in"; // default
                            // Check known port names
                            if (mPendingDragLink.portName == "entity") targetPort = "entity";
                            compound->add(std::make_unique<CreateLinkCommand>(
                                mPendingDragLink.nodeName, mPendingDragLink.portName, name, targetPort));
                        } else {
                            // Dragged from input → connect from first output of new node
                            std::string srcPort = "out";
                            if (mPendingDragLink.portName == "entity") srcPort = "entity";
                            compound->add(std::make_unique<CreateLinkCommand>(
                                name, srcPort, mPendingDragLink.nodeName, mPendingDragLink.portName));
                        }
                        CommandManager::instance().execute(std::move(compound));
                        mPendingDragLink.active = false;
                    } else {
                        CommandManager::instance().execute(
                            std::make_unique<CreateNodeCommand>(info->typeName, name, mLua));
                    }
                    mPendingPositions.push_back({name, mQuickAddPos.x, mQuickAddPos.y});
                    ImGui::CloseCurrentPopup();
                }
                idx++;
            }
        }

        // Arrow key navigation
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && mQuickAddSelection > 0) mQuickAddSelection--;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) mQuickAddSelection++;
        if (mQuickAddSelection >= idx) mQuickAddSelection = idx > 0 ? idx - 1 : 0;

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();

        if (idx == 0) ImGui::TextDisabled("No results");

        ImGui::EndPopup();
    }

    // ── Group context menu popup ────────────────────────────────────────────
    if (ImGui::BeginPopup("GroupContextMenu") && sGrpCtxIdx >= 0 &&
        sGrpCtxIdx < static_cast<int>(mNodeGroups.size())) {
        auto& grp = mNodeGroups[sGrpCtxIdx];

        ImGui::TextDisabled("Group: %s (%d nodes)", grp.name.c_str(),
                            static_cast<int>(grp.memberNames.size()));
        ImGui::Separator();

        // Rename
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText("Name", sGrpRenameBuf, sizeof(sGrpRenameBuf),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            grp.name = sGrpRenameBuf;
        }

        // Color
        ImGui::SliderFloat("Color", &grp.hue, 0.0f, 1.0f);
        ImGui::SameLine();
        ImVec4 preview = ImColor::HSV(grp.hue, 0.5f, 0.7f);
        ImGui::ColorButton("##hue", preview);

        ImGui::Separator();

        if (ImGui::MenuItem("Select all members")) {
            ned::ClearSelection();
            for (auto& name : grp.memberNames) {
                auto it = mNodes.find(name);
                if (it != mNodes.end()) ned::SelectNode(it->second.id, true);
            }
            mSelectedNodes.clear();
            for (auto& name : grp.memberNames) mSelectedNodes.insert(name);
            mSelectedNode = mSelectedNodes.empty() ? "" : *mSelectedNodes.begin();
            if (mSelectionCallback) mSelectionCallback(mSelectedNode);
        }

        if (mSelectedNodes.size() > 0 && ImGui::MenuItem("Add selection to group")) {
            for (auto& name : mSelectedNodes) {
                if (std::find(grp.memberNames.begin(), grp.memberNames.end(), name) == grp.memberNames.end())
                    grp.memberNames.push_back(name);
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Ungroup (keep nodes)")) {
            mNodeGroups.erase(mNodeGroups.begin() + sGrpCtxIdx);
            sGrpCtxIdx = -1;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Delete group + all nodes")) {
            auto compound = std::make_unique<CompoundCommand>("Delete group " + grp.name);
            for (auto& name : grp.memberNames) {
                if (mNodes.count(name))
                    compound->add(std::make_unique<DeleteNodeCommand>(name, mLua));
            }
            CommandManager::instance().execute(std::move(compound));
            mNodeGroups.erase(mNodeGroups.begin() + sGrpCtxIdx);
            sGrpCtxIdx = -1;
            mSelectedNodes.clear();
            mSelectedNode.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    } else if (!ImGui::IsPopupOpen("GroupContextMenu")) {
        sGrpCtxIdx = -1; // reset when popup closed
    }

    // ── Comment edit popup (outside ned scope) ─────────────────────────────
    if (mShowCommentPopup) {
        ImGui::OpenPopup("EditComment");
        mShowCommentPopup = false;
    }
    if (ImGui::BeginPopupModal("EditComment", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Comment for: %s", mCommentTarget.c_str());
        ImGui::SetNextItemWidth(300);
        ImGui::InputTextMultiline("##commentText", mCommentBuf, sizeof(mCommentBuf), {300, 100});
        ImGui::Separator();
        if (ImGui::Button("OK", {80, 0})) {
            std::string text(mCommentBuf);
            if (text.empty()) {
                mNodeComments.erase(mCommentTarget);
            } else {
                mNodeComments[mCommentTarget] = text;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {80, 0})) {
            ImGui::CloseCurrentPopup();
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

void NodeEditorPanel::positionNodeNextTo(const std::string& newNodeName, const std::string& targetNodeName) {
    ned::SetCurrentEditor(mEditorContext);
    auto tIt = mNodes.find(targetNodeName);
    if (tIt != mNodes.end()) {
        auto tpos = ned::GetNodePosition(tIt->second.id);
        auto tsz = ned::GetNodeSize(tIt->second.id);
        float px = tpos.x + tsz.x + 50;
        float py = tpos.y;  // align horizontally with target node

        // Avoid stacking: check ALL nodes (existing + pending) and shift down
        constexpr float kNodeHeight = 80.0f;
        bool shifted = true;
        while (shifted) {
            shifted = false;
            for (auto& [name, nd] : mNodes) {
                if (name == newNodeName) continue;
                auto npos = ned::GetNodePosition(nd.id);
                if (std::abs(npos.x - px) < 50 && std::abs(npos.y - py) < kNodeHeight) {
                    py = npos.y + kNodeHeight;
                    shifted = true;
                }
            }
            for (auto& pp : mPendingPositions) {
                if (std::abs(pp.x - px) < 50 && std::abs(pp.y - py) < kNodeHeight) {
                    py = pp.y + kNodeHeight;
                    shifted = true;
                }
            }
        }

        mPendingPositions.push_back({newNodeName, px, py});
    } else {
        mPendingPositions.push_back({newNodeName, 0, 0});
    }
    ned::SetCurrentEditor(nullptr);
}

} // namespace bbfx
