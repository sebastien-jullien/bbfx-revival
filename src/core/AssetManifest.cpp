#include "AssetManifest.h"
#include "Sha256.h"
#include "../network/HttpClient.h"
#include <OgreTextureManager.h>
#include <OgreException.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <sol/sol.hpp>

#ifdef _WIN32
#  include <shlobj.h>
#endif

namespace bbfx {

AssetManifest& AssetManifest::instance() {
    static AssetManifest s;
    return s;
}

// v3.5.2 final: SHA-256 (RFC 6234) via self-contained implementation in
// `Sha256.h/.cpp`. Replaces the earlier FNV-1a 64-bit placeholder. Output
// remains a hex string but jumps to 64 chars (256 bit). All cache paths
// derived from `getCachePath(hash)` use the leading 2 hex chars as the
// shard prefix (`<root>/<hash[0..1]>/<hash>`), so the layout transition
// is transparent — any cached entry just needs to be re-hashed once on
// first project load (legacy 16-char hashes are silently rejected and
// will be recomputed at the next save).
std::string AssetManifest::computeFileHash(const std::string& filepath) {
    return Sha256::hashFile(filepath);
}

std::string AssetManifest::computeBufferHash(const void* data, size_t size) {
    return Sha256::hashBuffer(data, size);
}

std::string AssetManifest::getCacheRoot() {
    std::string home;
#ifdef _WIN32
    char path[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_PROFILE, nullptr, 0, path) == S_OK) {
        home = path;
    } else if (const char* up = std::getenv("USERPROFILE")) {
        home = up;
    }
#else
    if (const char* h = std::getenv("HOME")) home = h;
#endif
    if (home.empty()) home = ".";
    std::string root = home + "/.bbfx/cache";
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    return root;
}

std::string AssetManifest::getCachePath(const std::string& hash) {
    if (hash.size() < 2) return std::string();
    std::string root = getCacheRoot();
    std::string sub = root + "/" + hash.substr(0, 2);
    std::error_code ec;
    std::filesystem::create_directories(sub, ec);
    return sub + "/" + hash;
}

bool AssetManifest::isCached(const std::string& hash) {
    if (hash.empty()) return false;
    std::error_code ec;
    return std::filesystem::exists(getCachePath(hash), ec);
}

std::string AssetManifest::resolve(const std::string& name) const {
    // v3.5.2 Sprint S7 Lot Y: return the OGRE-friendly filename so consumers
    // can pass it directly to TextureManager / TextureUnitState. The pipeline
    // copies the source file under that name into the recursive ResourceGroup
    // "AssetCache" registered by Engine::loadResources, so OGRE finds it.
    //
    // For consumers that need the actual on-disk path (unusual — most VJ
    // workflows want OGRE-loadable names), use `getCachePath(entry.hash)`.
    auto it = mEntries.find(name);
    if (it == mEntries.end()) return std::string();
    return it->second.filename;
}

std::string AssetManifest::resolveAndLoad(const std::string& name) const {
    // Resolves the logical name to its OGRE filename AND ensures the texture
    // is actually loaded into OGRE's TextureManager. If the cache is populated
    // and the recursive ResourceLocation is registered, OGRE will find the
    // file by name automatically. If not, this is a graceful no-op.
    std::string filename = resolve(name);
    if (filename.empty()) return std::string();

    auto& tm = Ogre::TextureManager::getSingleton();
    if (!tm.getByName(filename).get()) {
        try {
            tm.load(filename, "AssetCache");
        } catch (const Ogre::Exception& e) {
            // Resource not in AssetCache yet (cache miss / pipeline not run).
            // Caller can decide what to do — we return the filename anyway
            // so the chain continues with a known-bad reference (visible OGRE
            // warning rather than silent empty string).
            (void)e;
        }
    }
    return filename;
}

void AssetManifest::addEntry(const Entry& e) {
    mEntries[e.name] = e;
}

bool AssetManifest::removeEntry(const std::string& name) {
    return mEntries.erase(name) > 0;
}

const AssetManifest::Entry* AssetManifest::getEntry(const std::string& name) const {
    auto it = mEntries.find(name);
    return (it == mEntries.end()) ? nullptr : &it->second;
}

void AssetManifest::clearAll() { mEntries.clear(); }

// ── HTTPS download (uses HttpClient under the hood) ──────────────────────
namespace {
    std::atomic<size_t> sMaxDownloadBytes{500ull * 1024ull * 1024ull}; // 500 MB default
}

size_t AssetManifest::getMaxDownloadBytes() { return sMaxDownloadBytes.load(); }
void   AssetManifest::setMaxDownloadBytes(size_t bytes) { sMaxDownloadBytes.store(bytes); }

std::string AssetManifest::downloadToCache(const std::string& url,
                                            const std::string& expectedHash,
                                            std::string& outError) {
    outError.clear();
    if (url.rfind("https://", 0) != 0) {
        outError = "Refused: URL must use HTTPS (got: " + url + ")";
        return std::string();
    }
    if (expectedHash.size() != 64) {
        outError = "Refused: expectedHash must be 64-char SHA-256 hex digest";
        return std::string();
    }

    std::string cachePath = getCachePath(expectedHash);
    if (cachePath.empty()) {
        outError = "Could not derive cache path";
        return std::string();
    }

    // Already in cache? Fast path.
    std::error_code ec;
    if (std::filesystem::exists(cachePath, ec)) {
        return cachePath;
    }

    // Synchronous download via HttpClient. The completion callback is
    // dispatched on the main thread by `pumpMainThread()`. We poll with
    // atomics to avoid grabbing any user-mutex from the callback (which
    // would deadlock against this very loop, since pumpMainThread is
    // called inline and would run the callback while we hold the mutex).
    std::atomic<bool> done{false};
    std::atomic<bool> success{false};
    std::string finalError;
    std::mutex errMtx;

    HttpDownloadRequest req;
    req.url = url;
    req.destPath = cachePath;
    req.expectedSha256 = expectedHash;
    req.timeoutSeconds = 120;
    req.onProgress = [](size_t /*downloaded*/, size_t total) {
        if (total > sMaxDownloadBytes.load()) {
            // We can't actually abort from inside curl progress here without
            // a non-zero return; HttpClient handles size guards if needed.
        }
    };
    req.onComplete = [&](bool ok, std::string /*path*/, std::string err) {
        {
            std::lock_guard lk(errMtx);
            finalError = err;
        }
        success.store(ok, std::memory_order_release);
        done.store(true, std::memory_order_release);
    };

    HttpClient::instance().download(req);

    // Pump main thread until completion or timeout. No outer mutex held
    // here — the callback is free to write its results.
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::seconds(req.timeoutSeconds + 5);
    while (!done.load(std::memory_order_acquire)
        && std::chrono::steady_clock::now() < deadline) {
        HttpClient::instance().pumpMainThread();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!done.load(std::memory_order_acquire)) {
        outError = "Download timeout";
        return std::string();
    }
    if (!success.load(std::memory_order_acquire)) {
        std::lock_guard lk(errMtx);
        outError = finalError.empty() ? "Download failed" : finalError;
        return std::string();
    }

    // Verify size against the cap (post-download).
    if (auto sz = std::filesystem::file_size(cachePath, ec); !ec) {
        if (sz > sMaxDownloadBytes.load()) {
            std::filesystem::remove(cachePath, ec);
            outError = "Downloaded file exceeds 500 MB cap (got "
                     + std::to_string(sz) + " bytes)";
            return std::string();
        }
    }

    return cachePath;
}

nlohmann::json AssetManifest::toJson() const {
    nlohmann::json j;
    j["version"] = 1;
    j["entries"] = nlohmann::json::array();
    for (auto& [name, e] : mEntries) {
        nlohmann::json je;
        je["name"]       = e.name;
        je["filename"]   = e.filename;
        je["hash"]       = e.hash;
        je["size_bytes"] = (uint64_t)e.size_bytes;
        je["type"]       = e.type;
        je["source_url"] = e.source_url;
        je["category"]   = e.category;
        je["license"]    = e.license;
        je["author"]     = e.author;
        j["entries"].push_back(je);
    }
    return j;
}

void AssetManifest::fromJson(const nlohmann::json& j) {
    mEntries.clear();
    if (!j.is_object() || !j.contains("entries") || !j["entries"].is_array()) return;
    for (auto& je : j["entries"]) {
        Entry e;
        e.name       = je.value("name", "");
        e.filename   = je.value("filename", "");
        e.hash       = je.value("hash", "");
        e.size_bytes = je.value("size_bytes", (uint64_t)0);
        e.type       = je.value("type", "");
        e.source_url = je.value("source_url", "");
        e.category   = je.value("category", "");
        e.license    = je.value("license", "");
        e.author     = je.value("author", "");
        if (!e.name.empty()) mEntries[e.name] = e;
    }
}

size_t AssetManifest::loadFromLuaFile(sol::state& lua, const std::string& luaPath) {
    std::error_code ec;
    if (!std::filesystem::exists(luaPath, ec)) {
        // Missing manifest is not an error — pipeline may not have been run yet.
        return 0;
    }
    sol::protected_function_result res = lua.safe_script_file(luaPath, sol::script_pass_on_error);
    if (!res.valid()) {
        sol::error err = res;
        std::cerr << "[AssetManifest] " << luaPath << ": " << err.what() << std::endl;
        return 0;
    }
    sol::table tbl = res;
    if (!tbl.valid()) {
        std::cerr << "[AssetManifest] " << luaPath << ": did not return a table" << std::endl;
        return 0;
    }

    size_t added = 0;
    for (auto& kv : tbl) {
        if (!kv.first.is<std::string>() || !kv.second.is<sol::table>()) continue;
        sol::table row = kv.second;
        Entry e;
        e.name       = kv.first.as<std::string>();
        e.filename   = row.get_or<std::string>("file", "");
        e.hash       = row.get_or<std::string>("hash_sha256", "");
        e.size_bytes = row.get_or<uint64_t>("size_bytes", 0u);
        e.type       = row.get_or<std::string>("type", "");
        e.source_url = row.get_or<std::string>("source_url", "");
        e.category   = row.get_or<std::string>("category", "");
        e.license    = row.get_or<std::string>("license", "");
        e.author     = row.get_or<std::string>("author", "");
        if (e.name.empty()) continue;
        // Validate hash length (64 hex chars for SHA-256). Accept empty (legacy)
        // but warn on malformed.
        if (!e.hash.empty() && e.hash.size() != 64) {
            std::cerr << "[AssetManifest] " << e.name
                      << ": invalid hash length " << e.hash.size() << " (expected 64)" << std::endl;
        }
        mEntries[e.name] = e;
        ++added;
    }
    return added;
}

} // namespace bbfx
