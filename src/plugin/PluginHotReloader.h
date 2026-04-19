#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace bbfx {

/// v3.5 Lot U — polls the user plugins directory for changes to
/// init.lua / nodes/*.lua / presets/*.lua and triggers a clean
/// disable + enable cycle on the affected plugin. Debounces at 500ms
/// so a save followed quickly by editor-swap does not reload twice.
///
/// Polling is intentional: a single mtime scan of ~100 plugins is
/// <1ms and avoids platform-specific filesystem watcher APIs.
class PluginHotReloader {
public:
    static PluginHotReloader& instance();

    /// Call once per frame (StudioApp::capture). Internally rate-limited
    /// to one scan every 500ms.
    void tick();

    /// Watch additional plugins dirs beyond the default userPluginsDir.
    /// Safe to call before the manager has loaded anything.
    void addWatchDir(const std::filesystem::path& p);

    void setEnabled(bool on) { mEnabled = on; }
    bool isEnabled() const   { return mEnabled; }

    /// Force a full rescan on the next tick (used after install/uninstall).
    void invalidateAll();

    /// Stats for tests / dbg introspection.
    size_t watchedFileCount() const;
    int    reloadsPerformedSinceStart() const { return mReloads; }

private:
    PluginHotReloader() = default;
    PluginHotReloader(const PluginHotReloader&) = delete;
    PluginHotReloader& operator=(const PluginHotReloader&) = delete;

    void scan();
    std::vector<std::filesystem::path> pluginLuaFiles(
        const std::filesystem::path& pluginDir) const;

    bool mEnabled = true;
    int  mReloads = 0;
    std::chrono::steady_clock::time_point mLastScan{};
    std::chrono::steady_clock::time_point mLastChange{};
    std::string                           mPendingPlugin;

    // watched: pluginId -> { file -> mtime }
    std::unordered_map<std::string,
        std::unordered_map<std::string, std::filesystem::file_time_type>> mWatched;

    std::vector<std::filesystem::path> mExtraDirs;
};

} // namespace bbfx
