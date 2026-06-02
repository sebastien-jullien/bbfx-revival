#include "EditCommands.h"
#include "../../core/Animator.h"
#include "../../core/ParamSpec.h"
#include "../../core/AnimationPort.h"

namespace bbfx {

// ── EditParamCommand (M11) ───────────────────────────────────────────────────

EditParamCommand::EditParamCommand(const std::string& nodeName, const std::string& paramName,
                                   const ParamValueSnapshot& oldVal, const ParamValueSnapshot& newVal)
    : mNodeName(nodeName), mParamName(paramName), mOld(oldVal), mNew(newVal) {}

void EditParamCommand::apply(const ParamValueSnapshot& v) {
    auto* animator = Animator::instance();
    if (!animator) return;
    auto* node = animator->getRegisteredNode(mNodeName);
    if (!node) return;
    auto* spec = node->getParamSpec();
    if (!spec) return;
    auto* pd = spec->getParam(mParamName);
    if (!pd) return;
    switch (v.type) {
        case ParamType::FLOAT: pd->floatVal = v.f; break;
        case ParamType::INT:   pd->intVal = v.i; break;
        case ParamType::BOOL:  pd->boolVal = v.b; break;
        case ParamType::COLOR: for (int k=0;k<4;++k) pd->colorVal[k]=v.col[k]; break;
        case ParamType::VEC3:  for (int k=0;k<3;++k) pd->vec3Val[k]=v.v3[k]; break;
        default:               pd->stringVal = v.s; break; // STRING/ENUM/MESH/TEXTURE/MATERIAL/...
    }
    // Sync vers le port DAG de même nom (FLOAT/INT/ENUM), comme l'Inspector.
    auto& inputs = node->getInputs();
    auto it = inputs.find(mParamName);
    if (it != inputs.end()) {
        if (v.type == ParamType::FLOAT) it->second->setValue(v.f);
        else if (v.type == ParamType::INT) it->second->setValue(static_cast<float>(v.i));
        else if (v.type == ParamType::ENUM) {
            int idx = 0;
            for (size_t c = 0; c < pd->choices.size(); ++c)
                if (pd->choices[c] == v.s) { idx = static_cast<int>(c); break; }
            it->second->setValue(static_cast<float>(idx));
        }
    }
}

void EditParamCommand::execute() { apply(mNew); }
void EditParamCommand::undo()    { apply(mOld); }
std::string EditParamCommand::description() const {
    return "Edit " + mNodeName + "." + mParamName;
}

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
