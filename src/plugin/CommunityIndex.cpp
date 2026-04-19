#include "plugin/CommunityIndex.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#include "network/HttpClient.h"
#include "plugin/PluginManager.h"

namespace fs = std::filesystem;

namespace bbfx {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool majorMinorAtLeast(const std::string& have, const std::string& want) {
    // Very small comparator: interpret "X.Y" or "X.Y.Z" (ignore prerelease).
    auto parse = [](const std::string& v) {
        int a = 0, b = 0;
        std::sscanf(v.c_str(), "%d.%d", &a, &b);
        return std::make_pair(a, b);
    };
    auto [ha, hb] = parse(have);
    auto [wa, wb] = parse(want);
    if (ha != wa) return ha > wa;
    return hb >= wb;
}

} // anonymous

CommunityIndex& CommunityIndex::instance() {
    static CommunityIndex inst;
    return inst;
}

void CommunityIndex::setIndexUrl(const std::string& url) { mIndexUrl = url; }
const std::string& CommunityIndex::indexUrl() const { return mIndexUrl; }
void CommunityIndex::setCacheTtlSeconds(int seconds) { mCacheTtlSeconds = seconds; }

const CommunityIndexEntry* CommunityIndex::findById(const std::string& id) const {
    for (const auto& e : mEntries) if (e.id == id) return &e;
    return nullptr;
}

std::string CommunityIndex::cacheDir() const {
    // The user plugins dir is `<Documents>/BBFx/plugins`; the community
    // cache sits beside it at `<Documents>/BBFx/.community-cache/`.
    fs::path userPlugins = PluginManager::instance().getUserPluginsDir();
    fs::path base = userPlugins.parent_path();   // Documents/BBFx
    fs::path cache = base / ".community-cache";
    std::error_code ec;
    if (!fs::exists(cache, ec)) fs::create_directories(cache, ec);
    return cache.string();
}

std::string CommunityIndex::cacheIndexPath() const {
    return (fs::path(cacheDir()) / "index.json").string();
}

std::string CommunityIndex::thumbnailCachePath(const std::string& id) const {
    return (fs::path(cacheDir()) / "thumbnails" / (id + ".png")).string();
}

bool CommunityIndex::loadCached() {
    mLastError.clear();
    auto path = cacheIndexPath();
    std::error_code ec;
    if (!fs::exists(path, ec)) return false;

    // Check TTL: if the file is older than mCacheTtlSeconds, consider it
    // stale (the caller will likely refresh).
    auto now = std::chrono::system_clock::now();
    auto ftime = fs::last_write_time(path, ec);
    if (ec) return false;
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - decltype(ftime)::clock::now() + now);
    auto ageSec = std::chrono::duration_cast<std::chrono::seconds>(now - sctp).count();
    bool fresh = ageSec < mCacheTtlSeconds;

    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::stringstream buf;
    buf << f.rdbuf();
    if (parse(buf.str())) {
        mLoaded = true;
        mLastUpdated = sctp;
        return fresh;
    }
    return false;
}

void CommunityIndex::refresh(std::function<void(bool)> onComplete) {
    HttpClient::instance().get(mIndexUrl, [this, onComplete](HttpResponse r) mutable {
        if (r.status != 200 || !r.error.empty()) {
            mLastError = r.error.empty()
                ? ("HTTP " + std::to_string(r.status))
                : r.error;
            if (onComplete) onComplete(false);
            return;
        }
        if (!parse(r.body)) {
            if (onComplete) onComplete(false);
            return;
        }
        // Persist the raw body to the cache.
        std::error_code ec;
        fs::create_directories(fs::path(cacheIndexPath()).parent_path(), ec);
        std::ofstream out(cacheIndexPath(), std::ios::binary | std::ios::trunc);
        if (out.is_open()) out << r.body;
        mLoaded = true;
        mLastUpdated = std::chrono::system_clock::now();
        mLastError.clear();
        if (onComplete) onComplete(true);
    }, 30);
}

void CommunityIndex::loadOrRefresh(std::function<void(bool)> onComplete) {
    if (loadCached()) {
        if (onComplete) onComplete(true);
        return;
    }
    refresh(std::move(onComplete));
}

bool CommunityIndex::loadFromJsonString(const std::string& jsonText, std::string& error) {
    if (!parse(jsonText)) {
        error = mLastError;
        return false;
    }
    mLoaded = true;
    mLastUpdated = std::chrono::system_clock::now();
    return true;
}

bool CommunityIndex::parse(const std::string& jsonText) {
    mEntries.clear();
    try {
        nlohmann::json j = nlohmann::json::parse(jsonText);
        if (!j.is_object() || !j.contains("plugins") || !j["plugins"].is_array()) {
            mLastError = "index.json: missing or malformed 'plugins' array";
            return false;
        }
        for (const auto& p : j["plugins"]) {
            CommunityIndexEntry e;
            if (p.contains("id"))           e.id          = p["id"].get<std::string>();
            if (p.contains("name"))         e.name        = p["name"].get<std::string>();
            if (p.contains("version"))      e.version     = p["version"].get<std::string>();
            if (p.contains("bbfx_version")) e.bbfxVersion = p["bbfx_version"].get<std::string>();
            if (p.contains("author")) {
                if (p["author"].is_string()) e.author = p["author"].get<std::string>();
                else if (p["author"].is_object() && p["author"].contains("name"))
                    e.author = p["author"]["name"].get<std::string>();
            }
            if (p.contains("description"))  e.description = p["description"].get<std::string>();
            if (p.contains("category"))     e.category    = p["category"].get<std::string>();
            if (p.contains("license"))      e.license     = p["license"].get<std::string>();
            if (p.contains("homepage"))     e.homepage    = p["homepage"].get<std::string>();
            if (p.contains("downloadUrl"))  e.downloadUrl = p["downloadUrl"].get<std::string>();
            if (p.contains("thumbnailUrl")) e.thumbnailUrl= p["thumbnailUrl"].get<std::string>();
            if (p.contains("readmeUrl"))    e.readmeUrl   = p["readmeUrl"].get<std::string>();
            if (p.contains("changelogUrl")) e.changelogUrl= p["changelogUrl"].get<std::string>();
            if (p.contains("sha256"))       e.sha256      = p["sha256"].get<std::string>();
            if (p.contains("size"))         e.size        = p["size"].get<uint64_t>();
            if (p.contains("installs"))     e.installs    = p["installs"].get<int>();
            if (p.contains("rating"))       e.rating      = p["rating"].get<float>();
            if (p.contains("ratingCount"))  e.ratingCount = p["ratingCount"].get<int>();
            if (p.contains("featured"))     e.featured    = p["featured"].get<bool>();
            if (p.contains("updatedAt"))    e.updatedAt   = p["updatedAt"].get<std::string>();
            if (p.contains("githubIssue"))  e.githubIssue = p["githubIssue"].get<int>();
            if (p.contains("tags") && p["tags"].is_array()) {
                for (const auto& t : p["tags"])
                    if (t.is_string()) e.tags.push_back(t.get<std::string>());
            }
            if (p.contains("permissions") && p["permissions"].is_array()) {
                for (const auto& pm : p["permissions"]) {
                    if (!pm.is_string()) continue;
                    auto parsed = permissionFromString(pm.get<std::string>());
                    if (parsed) e.permissions.push_back(*parsed);
                }
            }
            mEntries.push_back(std::move(e));
        }
        mLastError.clear();
        return true;
    } catch (const std::exception& ex) {
        mLastError = std::string("index.json parse: ") + ex.what();
        return false;
    }
}

std::vector<const CommunityIndexEntry*>
CommunityIndex::filtered(const Filter& f) const {
    std::vector<const CommunityIndexEntry*> out;
    const std::string needle = toLower(f.search);

    for (const auto& e : mEntries) {
        if (!needle.empty()) {
            bool match = toLower(e.name).find(needle) != std::string::npos
                       || toLower(e.description).find(needle) != std::string::npos
                       || toLower(e.author).find(needle) != std::string::npos
                       || toLower(e.id).find(needle) != std::string::npos;
            for (const auto& tag : e.tags) {
                if (match) break;
                if (toLower(tag).find(needle) != std::string::npos) match = true;
            }
            if (!match) continue;
        }
        if (!f.categories.empty()) {
            bool hit = false;
            for (const auto& c : f.categories) if (c == e.category) { hit = true; break; }
            if (!hit) continue;
        }
        if (!f.licenses.empty()) {
            bool hit = false;
            for (const auto& l : f.licenses) if (l == e.license) { hit = true; break; }
            if (!hit) continue;
        }
        if (!f.author.empty()) {
            if (toLower(e.author).find(toLower(f.author)) == std::string::npos) continue;
        }
        if (f.minRating > 0.0f && e.rating < f.minRating) continue;
        if (!f.minBbfxVersion.empty()) {
            // entry.bbfxVersion is a CONSTRAINT ("^3.5"); interpret it
            // permissively here — include if the numeric part is at-least
            // the requested floor.
            std::string raw = e.bbfxVersion;
            while (!raw.empty() && (raw.front() == '>' || raw.front() == '=' ||
                                     raw.front() == '^' || raw.front() == '~' ||
                                     raw.front() == ' ')) raw.erase(raw.begin());
            if (!raw.empty() && !majorMinorAtLeast(raw, f.minBbfxVersion)) continue;
        }
        out.push_back(&e);
    }

    auto& sort = f.sortMode;
    std::sort(out.begin(), out.end(),
        [sort](const CommunityIndexEntry* a, const CommunityIndexEntry* b) {
            switch (sort) {
                case 1: return a->installs > b->installs;
                case 2: return a->updatedAt > b->updatedAt;        // ISO-8601 lex sort
                case 3: return a->rating > b->rating;
                case 4: return a->updatedAt > b->updatedAt;         // New = updated desc
                case 5: return a->name < b->name;
                case 0: default: {
                    // Trending: weight installs by recency (crude: rating × installs).
                    float sa = (a->rating + 0.1f) * (a->installs + 1);
                    float sb = (b->rating + 0.1f) * (b->installs + 1);
                    return sa > sb;
                }
            }
        });
    return out;
}

} // namespace bbfx
