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
