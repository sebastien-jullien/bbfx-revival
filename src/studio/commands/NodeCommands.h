#pragma once

#include "CommandManager.h"
#include <string>
#include <vector>
#include <sol/forward.hpp>

#include <vector>

namespace bbfx {

/// Deferred deletion queue — filled by DeleteNodeCommand, processed by StudioApp::renderFrame()
extern std::vector<std::string> gPendingDeletes;

class NodeEditorPanel; // forward
void setNodeEditorForCommands(NodeEditorPanel* panel);

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
    float mSavedPosX = 0, mSavedPosY = 0;
    bool mHasSavedPos = false;
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
    float mSavedPosX = 0, mSavedPosY = 0;
    bool mHasSavedPos = false;
};

} // namespace bbfx
