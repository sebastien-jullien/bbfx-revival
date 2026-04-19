#pragma once

#include <string>

#include "CommandManager.h"

namespace bbfx {

/// v3.5 Lot D — Plugin lifecycle actions as undoable commands.
///
/// The four commands mirror PluginManager::{enable,disable,unload,load} so
/// that the Studio's Undo/Redo stack can step back through plugin state
/// changes triggered from the UI (PluginManagerPanel in Lot G) or from
/// Lua scripts that wrap their side-effects in CommandManager calls.
///
/// Semantics:
///   - Enable  : execute() -> enable(id)   , undo() -> disable(id)
///   - Disable : execute() -> disable(id)  , undo() -> enable(id)
///   - Load    : execute() -> load(id)     , undo() -> unload(id)
///   - Unload  : execute() -> unload(id)   , undo() -> load(id)
///
/// PluginInstallCommand / PluginUninstallCommand require the full install
/// pipeline (HttpClient + ZipExtractor) which land in Lot F. Stubs are
/// declared here but throw until the pipeline is wired.

class PluginEnableCommand : public Command {
public:
    explicit PluginEnableCommand(std::string id) : mId(std::move(id)) {}
    void execute() override;
    void undo() override;
    std::string description() const override { return "Enable plugin " + mId; }
private:
    std::string mId;
    bool mWasEnabled = false;  // captured at execute() for undo safety
};

class PluginDisableCommand : public Command {
public:
    explicit PluginDisableCommand(std::string id) : mId(std::move(id)) {}
    void execute() override;
    void undo() override;
    std::string description() const override { return "Disable plugin " + mId; }
private:
    std::string mId;
    bool mWasEnabled = true;
};

class PluginLoadCommand : public Command {
public:
    explicit PluginLoadCommand(std::string id) : mId(std::move(id)) {}
    void execute() override;
    void undo() override;
    std::string description() const override { return "Load plugin " + mId; }
private:
    std::string mId;
    bool mWasLoaded = false;
};

class PluginUnloadCommand : public Command {
public:
    explicit PluginUnloadCommand(std::string id) : mId(std::move(id)) {}
    void execute() override;
    void undo() override;
    std::string description() const override { return "Unload plugin " + mId; }
private:
    std::string mId;
    bool mWasLoaded = true;
};

} // namespace bbfx
