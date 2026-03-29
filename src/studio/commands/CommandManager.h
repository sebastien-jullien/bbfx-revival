#pragma once

#include <memory>
#include <deque>
#include <string>

namespace bbfx {

class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string description() const = 0;
};

class CommandManager {
public:
    static CommandManager& instance();

    void execute(std::unique_ptr<Command> cmd);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    std::string undoDescription() const;
    std::string redoDescription() const;
    void clear();

private:
    CommandManager() = default;
    std::deque<std::unique_ptr<Command>> mUndoStack;
    std::deque<std::unique_ptr<Command>> mRedoStack;
    static constexpr size_t kMaxUndoDepth = 100;
};

} // namespace bbfx
