#include "EditCommands.h"
#include "../../core/Animator.h"

namespace bbfx {

// ── EditPortValueCommand ─────────────────────────────────────────────────────

EditPortValueCommand::EditPortValueCommand(const std::string& nodeName,
                                           const std::string& portName,
                                           float oldValue, float newValue)
    : mNodeName(nodeName), mPortName(portName),
      mOldValue(oldValue), mNewValue(newValue) {}

void EditPortValueCommand::setPortValue(float val) {
    auto* animator = Animator::instance();
    if (!animator) return;
    auto* node = animator->getRegisteredNode(mNodeName);
    if (!node) return;
    auto& inputs = node->getInputs();
    auto it = inputs.find(mPortName);
    if (it != inputs.end()) {
        it->second->setValue(val);
    }
}

void EditPortValueCommand::execute() { setPortValue(mNewValue); }
void EditPortValueCommand::undo()    { setPortValue(mOldValue); }

std::string EditPortValueCommand::description() const {
    return "Edit " + mNodeName + "." + mPortName;
}

// ── SetEnabledCommand ────────────────────────────────────────────────────────

SetEnabledCommand::SetEnabledCommand(const std::string& nodeName,
                                     bool oldEnabled, bool newEnabled)
    : mNodeName(nodeName), mOldEnabled(oldEnabled), mNewEnabled(newEnabled) {}

void SetEnabledCommand::execute() {
    auto* animator = Animator::instance();
    if (!animator) return;
    auto* node = animator->getRegisteredNode(mNodeName);
    if (node) node->setEnabled(mNewEnabled);
}

void SetEnabledCommand::undo() {
    auto* animator = Animator::instance();
    if (!animator) return;
    auto* node = animator->getRegisteredNode(mNodeName);
    if (node) node->setEnabled(mOldEnabled);
}

std::string SetEnabledCommand::description() const {
    return std::string(mNewEnabled ? "Enable" : "Disable") + " '" + mNodeName + "'";
}

// ── RenameNodeCommand ────────────────────────────────────────────────────────

RenameNodeCommand::RenameNodeCommand(const std::string& oldName,
                                     const std::string& newName)
    : mOldName(oldName), mNewName(newName) {}

void RenameNodeCommand::execute() {
    auto* animator = Animator::instance();
    if (animator) animator->renameNode(mOldName, mNewName);
}

void RenameNodeCommand::undo() {
    auto* animator = Animator::instance();
    if (animator) animator->renameNode(mNewName, mOldName);
}

std::string RenameNodeCommand::description() const {
    return "Rename '" + mOldName + "' → '" + mNewName + "'";
}

} // namespace bbfx
