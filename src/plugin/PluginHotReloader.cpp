#include "PluginHotReloader.h"

#include <iostream>

#include "PluginManager.h"

namespace bbfx {

namespace fs = std::filesystem;

PluginHotReloader& PluginHotReloader::instance() {
    static PluginHotReloader inst;
    return inst;
}

void PluginHotReloader::addWatchDir(const fs::path& p) {
    mExtraDirs.push_back(p);
}

void PluginHotReloader::invalidateAll() {
    mWatched.clear();
}

size_t PluginHotReloader::watchedFileCount() const {
    size_t n = 0;
    for (auto& kv : mWatched) n += kv.second.size();
    return n;
}

std::vector<fs::path> PluginHotReloader::pluginLuaFiles(const fs::path& pluginDir) const {
    std::vector<fs::path> out;
    std::error_code ec;
    if (!fs::is_directory(pluginDir, ec)) return out;
    // init.lua at the root
    fs::path init = pluginDir / "init.lua";
    if (fs::exists(init, ec)) out.push_back(init);
    for (auto& sub : { "nodes", "presets" }) {
        fs::path subDir = pluginDir / sub;
        if (!fs::is_directory(subDir, ec)) continue;
        for (auto& entry : fs::directory_iterator(subDir, ec)) {
            if (entry.path().extension() == ".lua") out.push_back(entry.path());
        }
    }
    return out;
}

void PluginHotReloader::tick() {
    if (!mEnabled) return;
    auto now = std::chrono::steady_clock::now();
    auto since = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now - mLastScan).count();
    if (since < 500) return;
    mLastScan = now;
    scan();
}

void PluginHotReloader::scan() {
    auto& pm = PluginManager::instance();
    auto ids = pm.listPlugins();

    std::vector<fs::path> roots = mExtraDirs;
    {
        std::error_code ec;
        fs::path user = pm.getUserPluginsDir();
        if (fs::is_directory(user, ec)) roots.push_back(user);
    }

    // For each installed plugin id, rescan its files and detect mtime delta.
    for (const auto& id : ids) {
        const auto* info = pm.getPlugin(id);
        if (!info) continue;
        fs::path dir = info->directoryPath;
        auto files = pluginLuaFiles(dir);
        auto& state = mWatched[id];
        bool anyChange = false;
        for (auto& f : files) {
            std::error_code ec;
            auto mt = fs::last_write_time(f, ec);
            if (ec) continue;
            std::string key = f.string();
            auto it = state.find(key);
            if (it == state.end()) {
                // First time we see it : store baseline, do not reload.
                state[key] = mt;
            } else if (it->second != mt) {
                it->second = mt;
                anyChange = true;
            }
        }
        if (anyChange) {
            mPendingPlugin = id;
            mLastChange = std::chrono::steady_clock::now();
        }
    }

    // If a change was observed, wait for quiet (debounce 500ms from last
    // mtime bump) before triggering disable + enable. The debounce
    // prevents reloading during a multi-file save burst.
    if (!mPendingPlugin.empty()) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - mLastChange).count();
        if (ms >= 500) {
            std::string id = std::move(mPendingPlugin);
            mPendingPlugin.clear();
            std::cout << "[HotReloader] reloading plugin " << id << std::endl;
            pm.disable(id);
            pm.unload(id);
            if (pm.load(id) && pm.enable(id)) {
                ++mReloads;
            } else {
                std::cerr << "[HotReloader] reload failed for " << id
                           << " — check Plugin Errors panel" << std::endl;
            }
        }
    }
}

} // namespace bbfx
