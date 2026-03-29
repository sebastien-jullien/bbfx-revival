#include "NodeCommands.h"
#include "../NodeTypeRegistry.h"
#include "../../core/Animator.h"
#include <iostream>

namespace bbfx {

// ── CreateNodeCommand ────────────────────────────────────────────────────────

CreateNodeCommand::CreateNodeCommand(const std::string& typeName,
                                     const std::string& nodeName,
                                     sol::state& lua)
    : mTypeName(typeName), mNodeName(nodeName), mLua(lua) {}

void CreateNodeCommand::execute() {
    auto* node = NodeTypeRegistry::instance().create(mTypeName, mNodeName, mLua);
    if (!node) {
        std::cerr << "[CreateNodeCommand] Failed to create " << mTypeName
                  << " '" << mNodeName << "'" << std::endl;
    }
    mExecuted = true;
}

void CreateNodeCommand::undo() {
    auto* animator = Animator::instance();
    if (!animator) return;
    auto* node = animator->getRegisteredNode(mNodeName);
    if (node) {
        animator->removeNode(node);
        delete node;
    }
}

std::string CreateNodeCommand::description() const {
    return "Create " + mTypeName + " '" + mNodeName + "'";
}

// ── DeleteNodeCommand ────────────────────────────────────────────────────────

DeleteNodeCommand::DeleteNodeCommand(const std::string& nodeName, sol::state& lua)
    : mNodeName(nodeName), mLua(lua) {}

void DeleteNodeCommand::execute() {
    auto* animator = Animator::instance();
    if (!animator) return;
    auto* node = animator->getRegisteredNode(mNodeName);
    if (!node) return;

    mTypeName = node->getTypeName();

    // Save input port values for undo
    mSavedInputs.clear();
    for (auto& [name, port] : node->getInputs()) {
        mSavedInputs.push_back({name, port->getValue()});
    }

    // Save links involving this node
    mSavedLinks.clear();
    for (auto& lk : animator->getLinks()) {
        if (lk.fromNode == mNodeName || lk.toNode == mNodeName) {
            mSavedLinks.push_back({lk.fromNode, lk.fromPort, lk.toNode, lk.toPort});
        }
    }

    animator->removeNode(node);
    delete node;
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
}

std::string DeleteNodeCommand::description() const {
    return "Delete '" + mNodeName + "'";
}

} // namespace bbfx
