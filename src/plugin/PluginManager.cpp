#include "plugin/PluginManager.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <sol/sol.hpp>

#include <OgreResourceGroupManager.h>

#include "plugin/InspectorWidgetRegistry.h"
#include "plugin/PluginErrorLog.h"
#include "plugin/PluginRegistry.h"
#include "plugin/PluginSandbox.h"
#include "plugin/PluginValidator.h"
#include "network/HttpClient.h"
#include "network/ZipExtractor.h"
#include "studio/NodeTypeRegistry.h"
#include "studio/ResourceEnumerator.h"
#include "studio/SettingsManager.h"

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace bbfx {

const char* toString(PluginState s) {
    switch (s) {
        case PluginState::DISCOVERED: return "DISCOVERED";
        case PluginState::VALIDATED:  return "VALIDATED";
        case PluginState::LOADED:     return "LOADED";
        case PluginState::ENABLED:    return "ENABLED";
        case PluginState::DISABLED:   return "DISABLED";
        case PluginState::UNLOADED:   return "UNLOADED";
        case PluginState::FAILED:     return "FAILED";
    }
    return "?";
}

PluginManager& PluginManager::instance() {
    static PluginManager inst;
    return inst;
}

static fs::path detectUserDocumentsDir() {
#ifdef _WIN32
    wchar_t* path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path)) && path) {
        fs::path p(path);
        CoTaskMemFree(path);
        return p;
    }
    return fs::current_path();
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    return home ? fs::path(home) / "Documents" : fs::current_path();
#endif
}

static fs::path detectExecutableDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return fs::path(std::wstring(buf, len)).parent_path();
    }
    return fs::current_path();
#else
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = 0;
        return fs::path(buf).parent_path();
    }
    return fs::current_path();
#endif
}

fs::path PluginManager::getUserPluginsDir() const {
    return detectUserDocumentsDir() / "BBFx" / "plugins";
}

fs::path PluginManager::getBundledPluginsDir() const {
    return detectExecutableDir() / "plugins";
}

void PluginManager::scanDirectories() {
    mPlugins.clear();

    fs::path userDir = getUserPluginsDir();
    fs::path bundledDir = getBundledPluginsDir();

    std::error_code ec;
    if (!fs::exists(userDir, ec)) {
        fs::create_directories(userDir, ec);
    }

    scanSingleDirectory(userDir, /*isBuiltin=*/false);
    scanSingleDirectory(bundledDir, /*isBuiltin=*/true);
}

void PluginManager::scanSingleDirectory(const fs::path& dir, bool isBuiltin) {
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_directory()) continue;

        fs::path manifestPath = entry.path() / "manifest.json";
        if (!fs::exists(manifestPath, ec)) continue;

        std::ifstream f(manifestPath);
        if (!f.is_open()) continue;

        nlohmann::json j;
        try {
            f >> j;
        } catch (const std::exception&) {
            continue;
        }

        std::string err;
        auto manifestOpt = PluginManifest::fromJson(j, err);
        if (!manifestOpt) continue;

        PluginInfo info;
        info.id = manifestOpt->id;
        info.directoryPath = entry.path();
        info.manifest = std::move(*manifestOpt);
        info.isBuiltin = isBuiltin;
        info.state = PluginState::DISCOVERED;
        info.resourceGroupName = "Plugin_" + info.id;
        info.installedAt = std::chrono::system_clock::now();

        // If we already saw this id from user dir, prefer the user version
        // (user installation overrides bundled).
        auto it = mPlugins.find(info.id);
        if (it != mPlugins.end() && !it->second.isBuiltin) continue;

        // Validate after basic parse.
        PluginValidator::Result res = PluginValidator::validate(info.directoryPath, info.manifest);
        if (res.ok) {
            info.state = PluginState::VALIDATED;
        } else {
            info.state = PluginState::FAILED;
            std::ostringstream oss;
            for (size_t i = 0; i < res.errors.size(); ++i) {
                if (i) oss << "; ";
                oss << res.errors[i];
            }
            info.lastError = oss.str();
        }

        mPlugins[info.id] = std::move(info);
    }
}

std::vector<std::string> PluginManager::listPlugins() const {
    std::vector<std::string> ids;
    ids.reserve(mPlugins.size());
    for (const auto& kv : mPlugins) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end(), [this](const std::string& a, const std::string& b) {
        const PluginInfo& pa = mPlugins.at(a);
        const PluginInfo& pb = mPlugins.at(b);
        return pa.manifest.name < pb.manifest.name;
    });
    return ids;
}

const PluginInfo* PluginManager::getPlugin(const std::string& id) const {
    auto it = mPlugins.find(id);
    return it == mPlugins.end() ? nullptr : &it->second;
}

PluginInfo* PluginManager::getPluginMutable(const std::string& id) {
    auto it = mPlugins.find(id);
    return it == mPlugins.end() ? nullptr : &it->second;
}

// ── Lifecycle (Lot B) ───────────────────────────────────────────────────────

void PluginManager::setLuaState(sol::state& lua) {
    mLua = &lua;
}

void PluginManager::tearDownRegistrations(PluginInfo& info) {
    // Remove every NodeTypeRegistry entry that was contributed by this plugin
    // before the sandbox is destroyed — otherwise the factory would still
    // hold a sol::function pointing at a dead environment.
    NodeTypeRegistry::instance().unregisterByPlugin(info.id);
    InspectorWidgetRegistry::instance().unregisterByPlugin(info.id);
    info.registeredNodeTypes.clear();
    info.registeredPresets.clear();
    PluginRegistry::instance().clearPlugin(info.id);
}

bool PluginManager::load(const std::string& id) {
    if (!mLua) {
        std::cerr << "[PluginManager] load(" << id << "): Lua state not set" << std::endl;
        return false;
    }
    PluginInfo* info = getPluginMutable(id);
    if (!info) return false;
    if (info->state == PluginState::LOADED || info->state == PluginState::ENABLED) {
        return true;  // already loaded
    }
    if (info->state != PluginState::VALIDATED && info->state != PluginState::DISABLED &&
        info->state != PluginState::UNLOADED) {
        info->lastError = "load refused in state " + std::string(toString(info->state));
        return false;
    }

    info->state = PluginState::DISCOVERED;  // reset intermediate

    // Create the OGRE resource group for this plugin up-front so the sandbox
    // load* APIs see a ready group when init.lua calls them. The group is
    // destroyed at unload(). ensureResourceGroup() inside
    // PluginSandboxApi.cpp remains idempotent as a safety net for plugins
    // that are loaded without going through this code path (tests).
    auto& rgm = Ogre::ResourceGroupManager::getSingleton();
    if (!rgm.resourceGroupExists(info->resourceGroupName)) {
        rgm.createResourceGroup(info->resourceGroupName, /*inGlobalPool=*/false);
    }
    bool locationKnown = false;
    try {
        const auto locs = rgm.listResourceLocations(info->resourceGroupName);
        for (const auto& l : *locs) {
            if (l == info->directoryPath.string()) { locationKnown = true; break; }
        }
    } catch (const std::exception&) { /* fresh group */ }
    if (!locationKnown) {
        try {
            rgm.addResourceLocation(info->directoryPath.string(), "FileSystem",
                                    info->resourceGroupName, /*recursive=*/true);
        } catch (const std::exception& e) {
            std::cerr << "[PluginManager] addResourceLocation " << id << ": "
                      << e.what() << std::endl;
        }
    }
    if (!rgm.isResourceGroupInitialised(info->resourceGroupName)) {
        try {
            rgm.initialiseResourceGroup(info->resourceGroupName);
        } catch (const std::exception& e) {
            std::cerr << "[PluginManager] initialiseResourceGroup " << id << ": "
                      << e.what() << std::endl;
        }
    }

    // Build the sandbox and execute init.lua.
    info->sandbox = PluginSandbox::create(*mLua, *info);

    std::filesystem::path entryPath = info->directoryPath / info->manifest.entry;
    auto result = mLua->safe_script_file(entryPath.string(), info->sandbox,
                                          sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        info->lastError = std::string("init.lua error: ") + err.what();
        info->state = PluginState::FAILED;
        std::cerr << "[PluginManager] " << id << ": " << info->lastError << std::endl;
        PluginErrorLog::instance().push(id, PluginErrorLog::Severity::Error,
                                         info->lastError, "load");
        tearDownRegistrations(*info);
        return false;
    }

    // init.lua may return a table with lifecycle hooks.
    sol::object ret = result;
    if (ret.is<sol::table>()) {
        sol::table t = ret;
        sol::object onLoad  = t["onLoad"];
        sol::object onEn    = t["onEnable"];
        sol::object onDis   = t["onDisable"];
        sol::object onUn    = t["onUnload"];
        if (onEn.is<sol::protected_function>()) info->onEnable  = onEn.as<sol::protected_function>();
        if (onDis.is<sol::protected_function>()) info->onDisable = onDis.as<sol::protected_function>();
        if (onUn.is<sol::protected_function>()) info->onUnload  = onUn.as<sol::protected_function>();
        if (onLoad.is<sol::protected_function>()) {
            auto cb = onLoad.as<sol::protected_function>();
            auto r = cb();
            if (!r.valid()) {
                sol::error err = r;
                info->lastError = std::string("onLoad error: ") + err.what();
                info->state = PluginState::FAILED;
                std::cerr << "[PluginManager] " << id << ": " << info->lastError << std::endl;
                PluginErrorLog::instance().push(id, PluginErrorLog::Severity::Error,
                                                 info->lastError, "onLoad");
                tearDownRegistrations(*info);
                return false;
            }
        }
    }

    // A sandbox violation fired from inside init.lua (e.g. require('ffi'))
    // already transitioned the plugin to FAILED via onSandboxViolation.
    // Do not regress that state back to LOADED — treat it as a load failure.
    if (info->state == PluginState::FAILED) {
        std::cerr << "[PluginManager] " << id << " load aborted: "
                  << info->lastError << std::endl;
        tearDownRegistrations(*info);
        return false;
    }

    info->state = PluginState::LOADED;
    info->lastUsedAt = std::chrono::system_clock::now();
    // New shaders/textures/materials/particles declared by init.lua become
    // visible in the AssetBrowser/Inspector dropdowns after this.
    ResourceEnumerator::invalidateCache();
    std::cout << "[PluginManager] " << id << " LOADED (init.lua ok)" << std::endl;
    return true;
}

bool PluginManager::enable(const std::string& id) {
    PluginInfo* info = getPluginMutable(id);
    if (!info) return false;
    if (info->state == PluginState::ENABLED) return true;
    if (info->state == PluginState::VALIDATED || info->state == PluginState::UNLOADED) {
        if (!load(id)) return false;
    }
    if (info->state != PluginState::LOADED && info->state != PluginState::DISABLED) {
        info->lastError = "enable refused in state " + std::string(toString(info->state));
        return false;
    }
    if (info->onEnable.valid()) {
        auto r = info->onEnable();
        if (!r.valid()) {
            sol::error err = r;
            info->lastError = std::string("onEnable error: ") + err.what();
            info->state = PluginState::FAILED;
            std::cerr << "[PluginManager] " << id << ": " << info->lastError << std::endl;
            PluginErrorLog::instance().push(id, PluginErrorLog::Severity::Error,
                                             info->lastError, "onEnable");
            return false;
        }
    }
    info->state = PluginState::ENABLED;
    info->lastUsedAt = std::chrono::system_clock::now();
    persistEnabled(id, true);
    std::cout << "[PluginManager] " << id << " ENABLED" << std::endl;
    return true;
}

bool PluginManager::disable(const std::string& id) {
    PluginInfo* info = getPluginMutable(id);
    if (!info) return false;
    if (info->state == PluginState::DISABLED) return true;
    if (info->state != PluginState::ENABLED && info->state != PluginState::LOADED) {
        return false;
    }
    if (info->onDisable.valid()) {
        auto r = info->onDisable();
        if (!r.valid()) {
            sol::error err = r;
            info->lastError = std::string("onDisable error: ") + err.what();
            // Keep going — disable must always move state forward to avoid leaks.
            std::cerr << "[PluginManager] " << id << ": " << info->lastError << std::endl;
        }
    }
    info->state = PluginState::DISABLED;
    persistEnabled(id, false);
    std::cout << "[PluginManager] " << id << " DISABLED" << std::endl;
    return true;
}

bool PluginManager::unload(const std::string& id) {
    PluginInfo* info = getPluginMutable(id);
    if (!info) return false;
    if (info->state == PluginState::ENABLED) disable(id);
    if (info->onUnload.valid()) {
        auto r = info->onUnload();
        if (!r.valid()) {
            sol::error err = r;
            info->lastError = std::string("onUnload error: ") + err.what();
            std::cerr << "[PluginManager] " << id << ": " << info->lastError << std::endl;
        }
    }

    // Drop all references from the plugin's environment before it becomes
    // unreachable — factories captured sol::function objects rooted in it.
    tearDownRegistrations(*info);
    info->onEnable = {};
    info->onDisable = {};
    info->onUnload = {};
    info->sandbox = sol::environment();

    auto& rgm = Ogre::ResourceGroupManager::getSingleton();
    if (rgm.resourceGroupExists(info->resourceGroupName)) {
        try {
            rgm.destroyResourceGroup(info->resourceGroupName);
        } catch (const std::exception& e) {
            std::cerr << "[PluginManager] unload " << id
                      << ": destroyResourceGroup: " << e.what() << std::endl;
        }
    }

    info->state = PluginState::UNLOADED;
    // Asset listings need to drop the plugin's shaders/textures/etc.
    ResourceEnumerator::invalidateCache();
    std::cout << "[PluginManager] " << id << " UNLOADED" << std::endl;
    return true;
}

bool PluginManager::uninstall(const std::string& id) {
    PluginInfo* info = getPluginMutable(id);
    if (!info) return false;
    if (info->isBuiltin) {
        std::cerr << "[PluginManager] uninstall " << id << ": builtin plugins cannot be uninstalled" << std::endl;
        return false;
    }
    // Unload first (idempotent if already disabled/unloaded).
    if (info->state == PluginState::ENABLED || info->state == PluginState::LOADED) {
        unload(id);
    }
    // Also scrub the persisted enabled list so a reinstall later does not
    // auto-enable a stale entry.
    persistEnabled(id, false);

    std::error_code ec;
    std::filesystem::remove_all(info->directoryPath, ec);
    if (ec) {
        std::cerr << "[PluginManager] uninstall " << id << ": remove_all failed: "
                  << ec.message() << std::endl;
        return false;
    }
    mPlugins.erase(id);
    std::cout << "[PluginManager] " << id << " UNINSTALLED" << std::endl;
    return true;
}

std::string PluginManager::installFromPath(const std::filesystem::path& src,
                                            std::string* errorOut) {
    auto setErr = [&](const std::string& msg) {
        if (errorOut) *errorOut = msg;
        std::cerr << "[PluginManager] installFromPath: " << msg << std::endl;
    };

    std::error_code ec;
    if (!std::filesystem::exists(src, ec)) {
        setErr("source does not exist: " + src.string());
        return {};
    }

    if (std::filesystem::is_regular_file(src, ec)) {
        // v3.5 Lot F: delegate to installFromZip so the pipeline is in
        // one place (temp extract -> validate -> move -> scan).
        std::string zErr;
        std::string id = installFromZip(src, &zErr);
        if (id.empty()) setErr(zErr);
        return id;
    }
    if (!std::filesystem::is_directory(src, ec)) {
        setErr("source is neither a directory nor a file: " + src.string());
        return {};
    }

    // Validate the source directory first so we don't create a half-copy.
    PluginValidator::Result res = PluginValidator::validatePath(src);
    if (!res.ok) {
        std::string joined;
        for (const auto& e : res.errors) { if (!joined.empty()) joined += "; "; joined += e; }
        setErr("source plugin is invalid: " + joined);
        return {};
    }

    // Read the manifest to get the id (we already know it is valid).
    std::ifstream f(src / "manifest.json");
    nlohmann::json j; f >> j;
    std::string err;
    auto manifestOpt = PluginManifest::fromJson(j, err);
    if (!manifestOpt) {
        setErr("manifest parse error: " + err);
        return {};
    }
    const std::string id = manifestOpt->id;

    std::filesystem::path dest = getUserPluginsDir() / id;
    if (std::filesystem::exists(dest, ec)) {
        setErr("destination already exists: " + dest.string() +
               " (uninstall first if you want to replace)");
        return {};
    }

    // Best-effort copy of the tree. std::filesystem::copy with recursive.
    try {
        std::filesystem::create_directories(dest);
        std::filesystem::copy(src, dest,
            std::filesystem::copy_options::recursive |
            std::filesystem::copy_options::copy_symlinks);
    } catch (const std::exception& e) {
        setErr(std::string("copy failed: ") + e.what());
        std::filesystem::remove_all(dest, ec);
        return {};
    }

    // Rescan so the new plugin appears in listPlugins() immediately.
    scanDirectories();
    std::cout << "[PluginManager] " << id << " INSTALLED from " << src << std::endl;
    return id;
}

std::string PluginManager::installFromZip(const std::filesystem::path& zipPath,
                                           std::string* errorOut) {
    auto setErr = [&](const std::string& msg) {
        if (errorOut) *errorOut = msg;
        std::cerr << "[PluginManager] installFromZip: " << msg << std::endl;
    };

    std::error_code ec;
    if (!std::filesystem::exists(zipPath, ec) ||
        !std::filesystem::is_regular_file(zipPath, ec)) {
        setErr("zip not found: " + zipPath.string());
        return {};
    }

    // Extract into a temp dir first. If any step below fails, the temp
    // dir is removed and no state leaks into the user plugins folder.
    namespace fs = std::filesystem;
    fs::path baseTemp = fs::temp_directory_path(ec);
    if (ec) baseTemp = fs::current_path();
    fs::path tempDir = baseTemp / ("bbfx_install_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    ZipExtractor::Options zopts;
    // Options kept strict — plugins should never need absolute paths or
    // traversal, and 100 MB is generous for a Lua+GLSL payload.
    std::string zErr;
    bool zok = ZipExtractor::extract(zipPath, tempDir, zopts, zErr);
    if (!zok) {
        fs::remove_all(tempDir, ec);
        setErr("zip extract failed: " + zErr);
        return {};
    }

    // The ZIP may wrap its content inside a top-level directory — accept
    // either layout. If exactly one entry is present and it is a dir that
    // contains manifest.json, treat it as the plugin root.
    fs::path pluginRoot = tempDir;
    if (!fs::exists(pluginRoot / "manifest.json", ec)) {
        std::vector<fs::path> children;
        for (auto& e : fs::directory_iterator(pluginRoot, ec)) {
            children.push_back(e.path());
        }
        if (children.size() == 1 && fs::is_directory(children[0], ec) &&
            fs::exists(children[0] / "manifest.json", ec)) {
            pluginRoot = children[0];
        } else {
            fs::remove_all(tempDir, ec);
            setErr("zip does not contain a manifest.json at the root (or one level deep)");
            return {};
        }
    }

    std::string id = installFromPath(pluginRoot, errorOut);
    fs::remove_all(tempDir, ec);
    if (id.empty()) return {};
    return id;
}

void PluginManager::installFromUrl(const std::string& url,
                                    const std::string& expectedSha256,
                                    std::function<void(bool, std::string, std::string)> onComplete) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path baseTemp = fs::temp_directory_path(ec);
    if (ec) baseTemp = fs::current_path();
    fs::path tmpZip = baseTemp / ("bbfx_download_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".zip");

    HttpDownloadRequest req;
    req.url = url;
    req.destPath = tmpZip.string();
    req.expectedSha256 = expectedSha256;
    req.timeoutSeconds = 300;
    req.onComplete = [this, tmpZip, onComplete](bool ok, std::string path, std::string error) {
        if (!ok) {
            std::error_code ec2;
            std::filesystem::remove(tmpZip, ec2);
            if (onComplete) onComplete(false, "", "download failed: " + error);
            return;
        }
        std::string zErr;
        std::string id = this->installFromZip(path, &zErr);
        std::error_code ec2;
        std::filesystem::remove(tmpZip, ec2);
        if (id.empty()) {
            if (onComplete) onComplete(false, "", zErr);
        } else {
            if (onComplete) onComplete(true, id, "");
        }
    };
    HttpClient::instance().download(req);
}

void PluginManager::persistEnabled(const std::string& id, bool enabled) {
    auto& s = SettingsManager::instance();
    auto settings = s.get();
    auto& list = settings.enabledPlugins;
    auto it = std::find(list.begin(), list.end(), id);
    bool changed = false;
    if (enabled) {
        if (it == list.end()) { list.push_back(id); changed = true; }
    } else {
        if (it != list.end()) { list.erase(it); changed = true; }
    }
    if (changed) {
        s.set(settings);
        s.save();
    }
}

void PluginManager::autoEnableFromSettings() {
    const auto& list = SettingsManager::instance().get().enabledPlugins;
    for (const auto& id : list) {
        auto* info = getPluginMutable(id);
        if (!info) {
            std::cerr << "[PluginManager] autoEnable: '" << id
                      << "' is in settings but not installed — skipped" << std::endl;
            continue;
        }
        if (info->state != PluginState::VALIDATED) continue;
        if (!enable(id)) {
            std::cerr << "[PluginManager] autoEnable: '" << id
                      << "' failed: " << info->lastError << std::endl;
        }
    }
}

void PluginManager::onSandboxViolation(const std::string& id, const std::string& detail) {
    PluginInfo* info = getPluginMutable(id);
    if (!info) {
        std::cerr << "[PluginManager] sandbox violation from unknown plugin " << id
                  << ": " << detail << std::endl;
        PluginErrorLog::instance().push(id, PluginErrorLog::Severity::Error,
                                         detail, "sandbox");
        return;
    }
    std::cerr << "[PluginManager] SANDBOX VIOLATION " << id << ": " << detail << std::endl;
    info->lastError = detail;
    PluginErrorLog::instance().push(id, PluginErrorLog::Severity::Error,
                                     detail, "sandbox");

    // If the plugin is currently active, disable it immediately. Keep the
    // state machine coherent — a failing plugin stays in FAILED until the
    // user explicitly unloads it.
    if (info->state == PluginState::ENABLED || info->state == PluginState::LOADED) {
        // Do not call onDisable here — a misbehaving plugin's own handler
        // cannot be trusted. Tear down registrations directly.
        tearDownRegistrations(*info);
    }
    info->state = PluginState::FAILED;
}

} // namespace bbfx
