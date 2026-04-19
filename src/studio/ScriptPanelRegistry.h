#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <sol/sol.hpp>

namespace bbfx {

/// v3.5 Lot P — registry of plugin-declared ImGui panels.
///
/// Populated by `bbfx.ui.registerPanel(title, drawFn)` inside a plugin
/// sandbox (requires the `ui` permission). StudioApp walks the list once
/// per frame, wraps each callback in ImGui::Begin/End, and toggles
/// visibility via a boolean shared with the panel entry (so plugins can
/// show/hide themselves as well as the user via the Windows menu).
class ScriptPanelRegistry {
public:
    static ScriptPanelRegistry& instance();

    /// Register a panel. `pluginId` is used to tear down all of a
    /// plugin's panels when it's disabled/uninstalled.
    void registerPanel(const std::string& pluginId,
                          const std::string& title,
                          sol::function drawFn);

    /// Remove a specific panel; safe if it does not exist.
    void unregisterPanel(const std::string& title);

    /// Remove every panel contributed by `pluginId` (called on disable).
    void unregisterByPlugin(const std::string& pluginId);

    /// Draw all registered panels (called from StudioApp::renderPanels
    /// each frame). Each panel wraps its callback in ImGui::Begin/End
    /// and respects the internal `visible` flag.
    void drawAll();

    /// List titles (for the Windows menu so the user can toggle panels).
    std::vector<std::string> titles() const;

    /// Toggle visibility of a panel — returns the new state, or false
    /// if the panel was not found.
    bool toggleVisible(const std::string& title);

private:
    ScriptPanelRegistry() = default;
    ScriptPanelRegistry(const ScriptPanelRegistry&) = delete;
    ScriptPanelRegistry& operator=(const ScriptPanelRegistry&) = delete;

    struct Entry {
        std::string   pluginId;
        std::string   title;
        sol::function drawFn;
        bool          visible = true;
    };
    mutable std::mutex mMutex;
    std::vector<Entry> mPanels;
};

} // namespace bbfx
