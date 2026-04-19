#pragma once

#include <functional>
#include <string>
#include <vector>

namespace bbfx {

/// v3.5 Lot H — Ctrl+Shift+P command palette (VS Code style).
///
/// The palette shows a centred modal overlay with a search field and a
/// scrollable list of actions. Currently exposed commands:
///   - Install <plugin>    — fuzzy-matches community index entries
///   - Enable  <plugin>    — cycles through installed + disabled plugins
///   - Disable <plugin>    — cycles through enabled plugins
///   - Uninstall <plugin>  — non-builtin installed plugins
///   - Open Community      — raise the community browser
///   - Open Plugin Manager — raise the plugin manager
///   - Refresh Community   — force an index re-fetch
///
/// New commands can be registered from any place via `registerCommand`.
class CommandPalette {
public:
    struct Command {
        std::string label;
        std::string detail;                 // grayed sub-text (optional)
        std::function<void()> action;
    };

    static CommandPalette& instance();

    void open();
    void close();
    bool isOpen() const { return mOpen; }

    void registerCommand(Command cmd);

    void draw();

private:
    CommandPalette();
    void rebuildDynamicCommands();

    bool mOpen = false;
    char mSearch[128] = {};
    int  mSelected = 0;
    std::vector<Command> mStaticCommands;
    std::vector<Command> mDynamicCommands;   // rebuilt each open
};

} // namespace bbfx
