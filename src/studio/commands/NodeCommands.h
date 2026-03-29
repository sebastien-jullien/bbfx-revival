#pragma once

#include "CommandManager.h"
#include <string>
#include <vector>
#include <sol/forward.hpp>

namespace bbfx {

class CreateNodeCommand : public Command {
public:
    CreateNodeCommand(const std::string& typeName, const std::string& nodeName,
                      sol::state& lua);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::string mTypeName;
    std::string mNodeName;
    sol::state& mLua;
    bool mExecuted = false;
};

class DeleteNodeCommand : public Command {
public:
    DeleteNodeCommand(const std::string& nodeName, sol::state& lua);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::string mNodeName;
    std::string mTypeName;
    sol::state& mLua;
    struct SavedPort { std::string name; float value; };
    std::vector<SavedPort> mSavedInputs;
    struct SavedLink { std::string fromNode, fromPort, toNode, toPort; };
    std::vector<SavedLink> mSavedLinks;
};

} // namespace bbfx
