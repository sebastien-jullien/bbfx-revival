#include "PluginCommands.h"

#include "../../plugin/PluginManager.h"

namespace bbfx {

static bool isEnabled(const std::string& id) {
    const auto* p = PluginManager::instance().getPlugin(id);
    return p && p->state == PluginState::ENABLED;
}

static bool isLoaded(const std::string& id) {
    const auto* p = PluginManager::instance().getPlugin(id);
    if (!p) return false;
    return p->state == PluginState::LOADED || p->state == PluginState::ENABLED;
}

// ── PluginEnableCommand ─────────────────────────────────────────────────────

void PluginEnableCommand::execute() {
    mWasEnabled = isEnabled(mId);
    if (!mWasEnabled) {
        PluginManager::instance().enable(mId);
    }
}

void PluginEnableCommand::undo() {
    // Only reverse the transition we actually performed; if the plugin was
    // already enabled before execute(), undo is a no-op (idempotency).
    if (!mWasEnabled) {
        PluginManager::instance().disable(mId);
    }
}

// ── PluginDisableCommand ────────────────────────────────────────────────────

void PluginDisableCommand::execute() {
    mWasEnabled = isEnabled(mId);
    if (mWasEnabled) {
        PluginManager::instance().disable(mId);
    }
}

void PluginDisableCommand::undo() {
    if (mWasEnabled) {
        PluginManager::instance().enable(mId);
    }
}

// ── PluginLoadCommand ───────────────────────────────────────────────────────

void PluginLoadCommand::execute() {
    mWasLoaded = isLoaded(mId);
    if (!mWasLoaded) {
        PluginManager::instance().load(mId);
    }
}

void PluginLoadCommand::undo() {
    if (!mWasLoaded) {
        PluginManager::instance().unload(mId);
    }
}

// ── PluginUnloadCommand ─────────────────────────────────────────────────────

void PluginUnloadCommand::execute() {
    mWasLoaded = isLoaded(mId);
    if (mWasLoaded) {
        PluginManager::instance().unload(mId);
    }
}

void PluginUnloadCommand::undo() {
    if (mWasLoaded) {
        PluginManager::instance().load(mId);
    }
}

} // namespace bbfx
