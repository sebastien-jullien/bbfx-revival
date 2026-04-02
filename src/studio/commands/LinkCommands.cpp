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
}

void CreateLinkCommand::undo() {
    unlinkPorts(mFromNode, mFromPort, mToNode, mToPort);
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
