#include "CommandManager.h"
#include <iostream>

namespace bbfx {

CommandManager& CommandManager::instance() {
    static CommandManager mgr;
    return mgr;
}

void CommandManager::execute(std::unique_ptr<Command> cmd) {
    cmd->execute();
    mUndoStack.push_back(std::move(cmd));
    mRedoStack.clear();
    if (mUndoStack.size() > kMaxUndoDepth) {
        mUndoStack.pop_front();
    }
}

void CommandManager::undo() {
    if (mUndoStack.empty()) return;
    auto cmd = std::move(mUndoStack.back());
    mUndoStack.pop_back();
    cmd->undo();
    mRedoStack.push_back(std::move(cmd));
}

void CommandManager::redo() {
    if (mRedoStack.empty()) return;
    auto cmd = std::move(mRedoStack.back());
    mRedoStack.pop_back();
    cmd->execute();
    mUndoStack.push_back(std::move(cmd));
}

bool CommandManager::canUndo() const { return !mUndoStack.empty(); }
bool CommandManager::canRedo() const { return !mRedoStack.empty(); }

std::string CommandManager::undoDescription() const {
    return mUndoStack.empty() ? "" : mUndoStack.back()->description();
}

std::string CommandManager::redoDescription() const {
    return mRedoStack.empty() ? "" : mRedoStack.back()->description();
}

void CommandManager::clear() {
    mUndoStack.clear();
    mRedoStack.clear();
}

} // namespace bbfx
