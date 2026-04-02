#pragma once

#include "CommandManager.h"
#include "../../core/ParamSpec.h"
#include <string>
#include <vector>
#include <sol/forward.hpp>

namespace bbfx {

class Animator;

/// Generate a readable name for a SceneObjectNode from a mesh filename.
/// Pattern: "Ogre", "Ogre.001", "Ogre.002" (auto-incremented suffix on duplicates).
std::string generateSceneObjectName(const std::string& meshFile, Animator* animator);

/// Duplicate a SceneObjectNode with all its ParamSpec values.
class DuplicateNodeCommand : public Command {
public:
    DuplicateNodeCommand(const std::string& sourceNodeName, sol::state& lua);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::string mSourceName;
    std::string mNewName;
    std::string mTypeName;
    std::vector<ParamDef> mParams;
    sol::state& mLua;
    bool mExecuted = false;
};

/// Reparent a SceneObjectNode under another SceneObjectNode (or root).
class ReparentNodeCommand : public Command {
public:
    ReparentNodeCommand(const std::string& childName,
                        const std::string& newParentName,
                        const std::string& oldParentName);
    void execute() override;
    void undo() override;
    std::string description() const override;
private:
    std::string mChildName, mNewParent, mOldParent;
};

} // namespace bbfx
