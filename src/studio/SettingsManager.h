#pragma once

#include <string>
#include <vector>

namespace bbfx {

struct Settings {
    int autoSaveInterval = 120;
    float defaultBPM = 120.0f;
    float viewportScale = 1.0f;
    int fontSize = 14;
    std::string lastProjectPath;
    std::vector<std::string> recentProjects;

    // v3.5 Lot D: plugin persistence
    std::vector<std::string> enabledPlugins;   // ids auto-enabled at startup
    int64_t pluginsLastScanAt = 0;             // unix epoch seconds
    int communityCacheTTL = 3600;              // seconds (Lot H uses this)

    // v3.5 Lot V — GitHub publishing. `githubToken` is stored XOR-scrambled
    // with a per-machine key; the raw OAuth token never lives on disk in
    // clear-text. `githubLogin` caches the authenticated username.
    std::string githubToken;
    std::string githubLogin;
};

class SettingsManager {
public:
    static SettingsManager& instance();

    const Settings& get() const { return mSettings; }
    void set(const Settings& s) { mSettings = s; }

    void load(const std::string& path = "");
    void save() const;

private:
    SettingsManager() = default;
    Settings mSettings;
    std::string mPath;
};

} // namespace bbfx
