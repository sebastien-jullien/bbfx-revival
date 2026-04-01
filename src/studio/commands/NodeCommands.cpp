#include "NodeCommands.h"
#include "../NodeTypeRegistry.h"
#include "../panels/NodeEditorPanel.h"
#include "../../core/Animator.h"
#include "../../fx/PerlinFxNode.h"
#include <iostream>

namespace bbfx {

// Deferred deletion queue
std::vector<std::string> gPendingDeletes;

// Static reference to the NodeEditorPanel for position save/restore
static NodeEditorPanel* sNodeEditorPanel = nullptr;
void setNodeEditorForCommands(NodeEditorPanel* panel) { sNodeEditorPanel = panel; }

// ── CreateNodeCommand ────────────────────────────────────────────────────────

CreateNodeCommand::CreateNodeCommand(const std::string& typeName,
                                     const std::string& nodeName,
                                     sol::state& lua)
    : mTypeName(typeName), mNodeName(nodeName), mLua(lua) {}

void CreateNodeCommand::execute() {
    AnimationNode* node = nullptr;
    try {
        node = NodeTypeRegistry::instance().create(mTypeName, mNodeName, mLua);
    } catch (const std::exception& e) {
        std::cerr << "[CreateNodeCommand] Exception creating " << mTypeName
                  << " '" << mNodeName << "': " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[CreateNodeCommand] Unknown exception creating " << mTypeName << std::endl;
    }
    if (!node) {
        std::cerr << "[CreateNodeCommand] Failed to create " << mTypeName
                  << " '" << mNodeName << "'" << std::endl;
    }
    // Restore saved position on redo
    if (mHasSavedPos && sNodeEditorPanel) {
        sNodeEditorPanel->setNodePositions({{mNodeName, mSavedPosX, mSavedPosY}});
    }
    mExecuted = true;
}

void CreateNodeCommand::undo() {
    // Save position before destroying
    if (sNodeEditorPanel) {
        for (auto& np : sNodeEditorPanel->getNodePositions()) {
            if (np.name == mNodeName) {
                mSavedPosX = np.x;
                mSavedPosY = np.y;
                mHasSavedPos = true;
                break;
            }
        }
    }
    auto* animator = Animator::instance();
    if (!animator) return;
    auto* node = animator->getRegisteredNode(mNodeName);
    if (node) {
        animator->removeNode(node);
        try { node->cleanup(); } catch (...) {}
    }
}

std::string CreateNodeCommand::description() const {
    return "Create " + mTypeName + " '" + mNodeName + "'";
}

// ── DeleteNodeCommand ────────────────────────────────────────────────────────

DeleteNodeCommand::DeleteNodeCommand(const std::string& nodeName, sol::state& lua)
    : mNodeName(nodeName), mLua(lua) {
}

void DeleteNodeCommand::execute() {
    auto* animator = Animator::instance();
    if (!animator) { std::cerr << "[DELETE] no animator" << std::endl; return; }
    auto* node = animator->getRegisteredNode(mNodeName);
    if (!node) { std::cerr << "[DELETE] node not found" << std::endl; return; }

    mTypeName = node->getTypeName();

    mSavedInputs.clear();
    for (auto& [name, port] : node->getInputs()) {
        mSavedInputs.push_back({name, port->getValue()});
    }

    mSavedLinks.clear();

    // Save node position — use try/catch since ned context may not be active
    if (sNodeEditorPanel) {
        try {
            auto positions = sNodeEditorPanel->getNodePositions();
            for (auto& np : positions) {
                if (np.name == mNodeName) {
                    mSavedPosX = np.x;
                    mSavedPosY = np.y;
                    mHasSavedPos = true;
                    break;
                }
            }
        } catch (...) {}
    }

    gPendingDeletes.push_back(mNodeName);
}

void DeleteNodeCommand::undo() {
    // Re-create the node
    auto* node = NodeTypeRegistry::instance().create(mTypeName, mNodeName, mLua);
    if (!node) return;

    // Restore input port values
    for (auto& sp : mSavedInputs) {
        auto& inputs = node->getInputs();
        auto it = inputs.find(sp.name);
        if (it != inputs.end()) {
            it->second->setValue(sp.value);
        }
    }

    // Restore links
    auto* animator = Animator::instance();
    if (!animator) return;
    for (auto& sl : mSavedLinks) {
        auto* fromNode = animator->getRegisteredNode(sl.fromNode);
        auto* toNode = animator->getRegisteredNode(sl.toNode);
        if (!fromNode || !toNode) continue;
        auto& outputs = fromNode->getOutputs();
        auto& inputs = toNode->getInputs();
        auto fromIt = outputs.find(sl.fromPort);
        auto toIt = inputs.find(sl.toPort);
        if (fromIt != outputs.end() && toIt != inputs.end()) {
            animator->link(fromIt->second, toIt->second);
        }
    }

    // Restore position in node editor
    if (mHasSavedPos && sNodeEditorPanel) {
        sNodeEditorPanel->setNodePositions({{mNodeName, mSavedPosX, mSavedPosY}});
    }
}

std::string DeleteNodeCommand::description() const {
    return "Delete '" + mNodeName + "'";
}

} // namespace bbfx
