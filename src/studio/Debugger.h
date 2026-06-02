#pragma once

#include <sol/sol.hpp>
#include <string>
#include <vector>
#include <functional>

namespace bbfx {

class StudioApp;

/// Studio Debugger — programmatic interface for testing and debugging.
/// Registers Lua functions under the `dbg` namespace accessible from the Console REPL.
/// Allows creating/deleting nodes, linking ports, editing parameters, taking screenshots,
/// and running automated test suites — all without mouse interaction.
class Debugger {
public:
    /// Install all debugger commands into the Lua state.
    /// Must be called after StudioApp and all panels are initialized.
    static void install(sol::state& lua, StudioApp* app);

    /// v3.5.2 Sprint S6 Lot X — register a callback fired after each preset load
    /// completes (the compound-command has executed). Receives the list of newly-
    /// created node names. Used by NodeEditorPanel to auto-layout fresh presets.
    /// Multiple callbacks accepted; called in registration order. No-op if no
    /// preset is loaded (regular `dbg.create` does NOT trigger the callback).
    using PresetLoadCallback = std::function<void(const std::vector<std::string>&)>;
    /// v3.5.2 Sprint S7 Lot AA — returns a stable callback id (>0) usable with
    /// unregisterPresetLoadCallback to remove the callback (avoids leaks in
    /// short-lived test/scripted contexts that re-register repeatedly).
    static int  registerPresetLoadCallback(PresetLoadCallback cb);
    static void unregisterPresetLoadCallback(int id);
};

} // namespace bbfx
