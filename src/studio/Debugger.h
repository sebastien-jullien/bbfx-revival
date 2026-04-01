#pragma once

#include <sol/sol.hpp>
#include <string>

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
};

} // namespace bbfx
