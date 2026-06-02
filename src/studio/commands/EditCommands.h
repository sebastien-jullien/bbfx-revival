#pragma once

#include "CommandManager.h"
#include "../../core/ParamSpec.h"
#include <string>

namespace bbfx {

// M11 — snapshot d'une valeur de ParamSpec (toutes les variantes de type).
struct ParamValueSnapshot {
    ParamType type = ParamType::FLOAT;
    float f = 0.0f; int i = 0; bool b = false;
    std::string s;
    float col[4] = {0, 0, 0, 0};
    float v3[3]  = {0, 0, 0};
};

// M11 — édition undoable d'un paramètre ParamSpec depuis l'Inspector. Capture la
// valeur avant/après et la ré-applique (+ sync vers le port DAG de même nom pour
// FLOAT/INT/ENUM, comme le fait l'Inspector). Avant, ces éditions étaient hors
// CommandManager → non annulables.
class EditParamCommand : public Command {
public:
    EditParamCommand(const std::string& nodeName, const std::string& paramName,
                     const ParamValueSnapshot& oldVal, const ParamValueSnapshot& newVal);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::string mNodeName, mParamName;
    ParamValueSnapshot mOld, mNew;
    void apply(const ParamValueSnapshot& v);
};

class EditPortValueCommand : public Command {
public:
    EditPortValueCommand(const std::string& nodeName, const std::string& portName,
                         float oldValue, float newValue);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::string mNodeName, mPortName;
    float mOldValue, mNewValue;
    void setPortValue(float val);
};

class SetEnabledCommand : public Command {
public:
    SetEnabledCommand(const std::string& nodeName, bool oldEnabled, bool newEnabled);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::string mNodeName;
    bool mOldEnabled, mNewEnabled;
};

class RenameNodeCommand : public Command {
public:
    RenameNodeCommand(const std::string& oldName, const std::string& newName);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::string mOldName, mNewName;
};

} // namespace bbfx
