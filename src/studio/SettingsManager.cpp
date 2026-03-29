#include "SettingsManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#include <shlobj.h>
#endif

using json = nlohmann::json;

namespace bbfx {

SettingsManager& SettingsManager::instance() {
    static SettingsManager mgr;
    return mgr;
}

static std::string getDefaultPath() {
#ifdef _WIN32
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata) == S_OK) {
        std::string dir = std::string(appdata) + "\\BBFx";
        std::filesystem::create_directories(dir);
        return dir + "\\settings.json";
    }
#endif
    return "bbfx-studio-settings.json";
}

void SettingsManager::load(const std::string& path) {
    mPath = path.empty() ? getDefaultPath() : path;
    if (!std::filesystem::exists(mPath)) return;
    try {
        std::ifstream f(mPath);
        json j = json::parse(f);
        if (j.contains("autoSaveInterval")) mSettings.autoSaveInterval = j["autoSaveInterval"];
        if (j.contains("defaultBPM"))       mSettings.defaultBPM = j["defaultBPM"];
        if (j.contains("viewportScale"))    mSettings.viewportScale = j["viewportScale"];
        if (j.contains("fontSize"))         mSettings.fontSize = j["fontSize"];
        if (j.contains("lastProjectPath"))  mSettings.lastProjectPath = j["lastProjectPath"];
        if (j.contains("recentProjects")) {
            mSettings.recentProjects.clear();
            for (auto& p : j["recentProjects"]) mSettings.recentProjects.push_back(p);
        }
    } catch (const std::exception& e) {
        std::cerr << "[Settings] Failed to load: " << e.what() << std::endl;
    }
}

void SettingsManager::save() const {
    std::string p = mPath.empty() ? getDefaultPath() : mPath;
    try {
        json j;
        j["autoSaveInterval"] = mSettings.autoSaveInterval;
        j["defaultBPM"] = mSettings.defaultBPM;
        j["viewportScale"] = mSettings.viewportScale;
        j["fontSize"] = mSettings.fontSize;
        j["lastProjectPath"] = mSettings.lastProjectPath;
        j["recentProjects"] = mSettings.recentProjects;
        std::ofstream f(p);
        f << j.dump(2);
    } catch (const std::exception& e) {
        std::cerr << "[Settings] Failed to save: " << e.what() << std::endl;
    }
}

} // namespace bbfx
