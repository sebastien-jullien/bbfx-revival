#pragma once

#include "CommandManager.h"
#include <string>

namespace bbfx {

class CreateLinkCommand : public Command {
public:
    CreateLinkCommand(const std::string& fromNode, const std::string& fromPort,
                      const std::string& toNode, const std::string& toPort);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::string mFromNode, mFromPort, mToNode, mToPort;
    // Auto entity→entity link (created automatically when linking data ports
    // between a SceneObjectNode and a node with entity input)
    std::string mAutoEntityFrom, mAutoEntityTo;
    bool mAutoEntityCreated = false;
};

class DeleteLinkCommand : public Command {
public:
    DeleteLinkCommand(const std::string& fromNode, const std::string& fromPort,
                      const std::string& toNode, const std::string& toPort);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::string mFromNode, mFromPort, mToNode, mToPort;
};

} // namespace bbfx
