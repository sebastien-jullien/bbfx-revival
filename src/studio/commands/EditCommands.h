#pragma once

#include "CommandManager.h"
#include <string>

namespace bbfx {

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
