#pragma once

#include <memory>
#include <deque>
#include <string>
#include <vector>
#include <functional>

namespace bbfx {

class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string description() const = 0;
};

/// Groups multiple commands into a single undoable operation
class CompoundCommand : public Command {
public:
    explicit CompoundCommand(const std::string& desc) : mDesc(desc) {}
    void add(std::unique_ptr<Command> cmd) { mCommands.push_back(std::move(cmd)); }
    void execute() override;
    void undo() override;
    std::string description() const override { return mDesc; }
private:
    std::string mDesc;
    std::vector<std::unique_ptr<Command>> mCommands;
};

/// Wraps a std::function pair as a Command (for one-off operations in CompoundCommand)
class LambdaCommand : public Command {
public:
    LambdaCommand(std::string desc, std::function<void()> doFn, std::function<void()> undoFn)
        : mDesc(std::move(desc)), mDoFn(std::move(doFn)), mUndoFn(std::move(undoFn)) {}
    void execute() override { if (mDoFn) mDoFn(); }
    void undo() override { if (mUndoFn) mUndoFn(); }
    std::string description() const override { return mDesc; }
private:
    std::string mDesc;
    std::function<void()> mDoFn, mUndoFn;
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
