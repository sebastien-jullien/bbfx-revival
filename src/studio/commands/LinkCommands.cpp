#include "LinkCommands.h"
#include "../../core/Animator.h"

namespace bbfx {

// ── helpers ──────────────────────────────────────────────────────────────────

static bool linkPorts(const std::string& fromNode, const std::string& fromPort,
                      const std::string& toNode, const std::string& toPort) {
    auto* animator = Animator::instance();
    if (!animator) return false;
    auto* fn = animator->getRegisteredNode(fromNode);
    auto* tn = animator->getRegisteredNode(toNode);
    if (!fn || !tn) return false;
    auto& outs = fn->getOutputs();
    auto& ins = tn->getInputs();
    auto oIt = outs.find(fromPort);
    auto iIt = ins.find(toPort);
    if (oIt == outs.end() || iIt == ins.end()) return false;
    animator->link(oIt->second, iIt->second);
    // Auto-fill target_entity and notify FX node
    if (fromPort == "entity" && toPort == "entity") {
        if (tn->getParamSpec()) {
            auto* td = tn->getParamSpec()->getParam("target_entity");
            if (td) td->stringVal = fromNode;
        }
        tn->onLinkChanged();
    }
    return true;
}

static bool unlinkPorts(const std::string& fromNode, const std::string& fromPort,
                        const std::string& toNode, const std::string& toPort) {
    auto* animator = Animator::instance();
    if (!animator) return false;
    auto* fn = animator->getRegisteredNode(fromNode);
    auto* tn = animator->getRegisteredNode(toNode);
    if (!fn || !tn) return false;
    auto& outs = fn->getOutputs();
    auto& ins = tn->getInputs();
    auto oIt = outs.find(fromPort);
    auto iIt = ins.find(toPort);
    if (oIt == outs.end() || iIt == ins.end()) return false;
    animator->unlink(oIt->second, iIt->second);
    // Clear target_entity and notify FX node
    if (fromPort == "entity" && toPort == "entity") {
        if (tn->getParamSpec()) {
            auto* td = tn->getParamSpec()->getParam("target_entity");
            if (td) td->stringVal.clear();
        }
        tn->onLinkChanged();
    }
    return true;
}

// ── CreateLinkCommand ────────────────────────────────────────────────────────

CreateLinkCommand::CreateLinkCommand(const std::string& fromNode,
                                     const std::string& fromPort,
                                     const std::string& toNode,
                                     const std::string& toPort)
    : mFromNode(fromNode), mFromPort(fromPort),
      mToNode(toNode), mToPort(toPort) {}

void CreateLinkCommand::execute() {
    linkPorts(mFromNode, mFromPort, mToNode, mToPort);

    // Auto-create entity→entity link when connecting data ports between
    // a SceneObjectNode (entity output) and a node with entity input
    if (mFromPort != "entity" && mToPort != "entity" && !mAutoEntityCreated) {
        auto* animator = Animator::instance();
        if (animator) {
            auto* fn = animator->getRegisteredNode(mFromNode);
            auto* tn = animator->getRegisteredNode(mToNode);
            if (fn && tn) {
                // Determine which node is the SceneObjectNode (entity output)
                // and which has the entity input
                std::string sceneNode, targetNode;
                if (tn->getOutputs().count("entity") && fn->getInputs().count("entity"))
                    { sceneNode = mToNode; targetNode = mFromNode; }
                else if (fn->getOutputs().count("entity") && tn->getInputs().count("entity"))
                    { sceneNode = mFromNode; targetNode = mToNode; }

                if (!sceneNode.empty()) {
                    // Check no entity link already exists between these two
                    bool exists = false;
                    for (auto& lk : animator->getLinks()) {
                        if (lk.fromNode == sceneNode && lk.fromPort == "entity" &&
                            lk.toNode == targetNode && lk.toPort == "entity") {
                            exists = true; break;
                        }
                    }
                    if (!exists) {
                        linkPorts(sceneNode, "entity", targetNode, "entity");
                        mAutoEntityFrom = sceneNode;
                        mAutoEntityTo = targetNode;
                        mAutoEntityCreated = true;
                    }
                }
            }
        }
    }
}

void CreateLinkCommand::undo() {
    unlinkPorts(mFromNode, mFromPort, mToNode, mToPort);
    if (mAutoEntityCreated) {
        unlinkPorts(mAutoEntityFrom, "entity", mAutoEntityTo, "entity");
        mAutoEntityCreated = false;
    }
}

std::string CreateLinkCommand::description() const {
    return "Link " + mFromNode + "." + mFromPort + " → " + mToNode + "." + mToPort;
}

// ── DeleteLinkCommand ────────────────────────────────────────────────────────

DeleteLinkCommand::DeleteLinkCommand(const std::string& fromNode,
                                     const std::string& fromPort,
                                     const std::string& toNode,
                                     const std::string& toPort)
    : mFromNode(fromNode), mFromPort(fromPort),
      mToNode(toNode), mToPort(toPort) {}

void DeleteLinkCommand::execute() {
    unlinkPorts(mFromNode, mFromPort, mToNode, mToPort);
}

void DeleteLinkCommand::undo() {
    linkPorts(mFromNode, mFromPort, mToNode, mToPort);
}

std::string DeleteLinkCommand::description() const {
    return "Delete link " + mFromNode + "." + mFromPort + " → " + mToNode + "." + mToPort;
}

} // namespace bbfx
