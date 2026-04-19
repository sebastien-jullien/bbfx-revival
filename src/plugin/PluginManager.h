#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <sol/forward.hpp>
#include <sol/environment.hpp>

#include "plugin/PluginManifest.h"

namespace bbfx {

enum class PluginState {
    DISCOVERED,
    VALIDATED,
    LOADED,
    ENABLED,
    DISABLED,
    UNLOADED,
    FAILED
};

const char* toString(PluginState s);

struct PluginInfo {
    std::string id;
    std::filesystem::path directoryPath;
    PluginManifest manifest;
    PluginState state = PluginState::DISCOVERED;
    bool isBuiltin = false;

    std::string resourceGroupName;
    std::vector<std::string> registeredNodeTypes;
    std::vector<std::string> registeredPresets;
    std::string lastError;

    // Lifecycle state populated by PluginManager::load / enable / ...
    // The environment is the isolated sol::environment the plugin runs in;
    // the four hooks are the functions returned from its init.lua (any of
    // them may be absent, in which case they stay invalid).
    sol::environment sandbox;
    sol::protected_function onEnable;
    sol::protected_function onDisable;
    sol::protected_function onUnload;

    std::chrono::system_clock::time_point installedAt;
    std::chrono::system_clock::time_point lastUsedAt;
};

class PluginManager {
public:
    static PluginManager& instance();

    // --- discovery (Lot A) -------------------------------------------------
    void scanDirectories();
    std::vector<std::string> listPlugins() const;
    const PluginInfo* getPlugin(const std::string& id) const;
    PluginInfo* getPluginMutable(const std::string& id);

    std::filesystem::path getUserPluginsDir() const;
    std::filesystem::path getBundledPluginsDir() const;

    // --- lifecycle (Lot B) -------------------------------------------------
    // These are no-ops until `setLuaState` has been called with the engine's
    // sol::state. Both the headless engine and the Studio call this during
    // their startup sequence.
    void setLuaState(sol::state& lua);

    // Transitions VALIDATED -> LOADED (runs init.lua) -> ENABLED (calls
    // onEnable if present). Returns true on success; on failure the plugin
    // ends up in FAILED with `lastError` populated.
    bool load(const std::string& id);
    bool enable(const std::string& id);
    bool disable(const std::string& id);
    bool unload(const std::string& id);

    // Called by the sandbox machinery when a plugin violates a rule. The
    // plugin is immediately transitioned to FAILED and future calls into it
    // become no-ops. Lot G wires this to ToastSystem + PluginErrorsPanel.
    void onSandboxViolation(const std::string& id, const std::string& detail);

    // Lot D: Walk `Settings.enabledPlugins` and call enable() on each id that
    // is currently VALIDATED. Called once at startup, after the settings
    // file has been loaded and plugins have been scanned. Non-fatal: a
    // plugin that fails to enable is logged and the loop continues.
    void autoEnableFromSettings();

    // Lot D: Pure setter used by PluginCommands and friends: updates the
    // Settings.enabledPlugins vector AND persists the settings file. Meant
    // for programmatic tests; the internal enable()/disable() code path
    // calls this automatically.
    void persistEnabled(const std::string& id, bool enabled);

    // Lot D: uninstall removes a plugin from disk. It disables and unloads
    // first so resources are cleaned up. Builtin (bundled) plugins cannot
    // be uninstalled — returns false. Returns true iff the plugin was
    // installed and successfully removed.
    bool uninstall(const std::string& id);

    // Lot D: install a plugin from a local directory or a .zip.
    //  - Directory: validated + copied into ~/Documents/BBFx/plugins/<id>/
    //  - .zip: extracted to a temp dir, validated, then moved into the
    //    user plugins dir (Lot F).
    // Returns the plugin id on success, empty string on failure.
    std::string installFromPath(const std::filesystem::path& src,
                                 std::string* errorOut = nullptr);

    // Lot F: dedicated ZIP install. Same semantics as installFromPath when
    // given a .zip. Separated so callers can signal intent clearly.
    std::string installFromZip(const std::filesystem::path& zipPath,
                                std::string* errorOut = nullptr);

    // Lot F: async install from URL. Downloads to a temp file, verifies
    // the SHA-256 if non-empty, extracts, validates, moves into place, and
    // finally invokes `onComplete(ok, id, error)` on the main thread.
    void installFromUrl(const std::string& url,
                        const std::string& expectedSha256,
                        std::function<void(bool ok, std::string id, std::string error)> onComplete);

private:
    PluginManager() = default;
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    void scanSingleDirectory(const std::filesystem::path& dir, bool isBuiltin);
    void tearDownRegistrations(PluginInfo& info);

    std::map<std::string, PluginInfo> mPlugins;
    sol::state* mLua = nullptr;
};

} // namespace bbfx
