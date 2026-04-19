#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "plugin/PluginManifest.h"  // PluginPermission

namespace bbfx {

/// A single entry in the community index fetched from the GitHub repo.
struct CommunityIndexEntry {
    std::string id;
    std::string name;
    std::string version;
    std::string bbfxVersion;    // constraint e.g. ">=3.5"
    std::string author;
    std::string description;
    std::string category;
    std::vector<std::string> tags;
    std::string license;
    std::string homepage;

    std::string downloadUrl;
    std::string thumbnailUrl;
    std::string readmeUrl;      // optional — we fetch on demand
    std::string changelogUrl;   // optional
    std::string sha256;
    uint64_t    size = 0;

    int    installs = 0;
    float  rating = 0.0f;        // 0..5 — from index.json
    int    ratingCount = 0;
    bool   featured = false;

    // v3.5 Lot I — optional GitHub Reactions issue number; set by the
    // CI that publishes the index, left 0 when the plugin isn't wired to
    // a discussion.
    int    githubIssue = 0;

    // v3.5 Lot I — live rating populated by GithubReactionsFetcher on
    // demand (when the user opens the details pane). When `liveRatingValid`
    // is true, renderers prefer it over `rating` for the star display.
    mutable float liveRating = 0.0f;
    mutable int   liveRatingCount = 0;
    mutable bool  liveRatingValid = false;
    std::vector<PluginPermission> permissions;

    std::string updatedAt;       // ISO-8601
};

/// v3.5 Lot H — in-memory community index.
///
/// Responsible for downloading the GitHub-hosted `index.json` (via
/// HttpClient), caching it under `%APPDATA%/BBFx/.community-cache/`, and
/// exposing a sortable/filterable view of the entries.
class CommunityIndex {
public:
    static CommunityIndex& instance();

    // ── configuration ─────────────────────────────────────────────────────
    // Default is the `bonneballefx/bbfx-community-plugins` repo on main.
    // Tests and mirrors can override this.
    void setIndexUrl(const std::string& url);
    const std::string& indexUrl() const;

    // Cache TTL (settings-driven, default 3600 s). Used by refresh() to
    // skip fetch when the on-disk copy is still fresh.
    void setCacheTtlSeconds(int seconds);

    // ── data access ───────────────────────────────────────────────────────
    const std::vector<CommunityIndexEntry>& entries() const { return mEntries; }
    const CommunityIndexEntry* findById(const std::string& id) const;
    size_t size() const { return mEntries.size(); }

    bool isLoaded() const { return mLoaded; }
    const std::string& lastError() const { return mLastError; }
    std::chrono::system_clock::time_point lastUpdated() const { return mLastUpdated; }

    // ── lifecycle ─────────────────────────────────────────────────────────
    // Tries to load the cached index from disk. Returns true if a fresh
    // copy was found (within TTL).
    bool loadCached();

    // Force a fetch — always hits the network. `onComplete(true)` on
    // success, `(false)` on network or parse error (`lastError()` updated).
    void refresh(std::function<void(bool ok)> onComplete);

    // Re-load from cache if present, otherwise refresh() over the network.
    void loadOrRefresh(std::function<void(bool ok)> onComplete);

    // ── filtering helpers ─────────────────────────────────────────────────
    struct Filter {
        std::string     search;       // case-insensitive, match name/desc/tags/author
        std::vector<std::string> categories;       // empty = all
        std::vector<std::string> licenses;         // empty = all
        std::string     author;       // exact (case-insensitive prefix)
        float           minRating = 0.0f;
        std::string     minBbfxVersion;            // e.g. "3.5"
        int             sortMode = 0;              // 0=Trending,1=Installs,2=Updated,3=Top,4=New,5=Name
    };
    std::vector<const CommunityIndexEntry*> filtered(const Filter& f) const;

    // ── paths ─────────────────────────────────────────────────────────────
    std::string cacheDir() const;
    std::string cacheIndexPath() const;
    std::string thumbnailCachePath(const std::string& id) const;

    // Populate a mock index from a raw JSON string (tests use this to avoid
    // hitting the network).
    bool loadFromJsonString(const std::string& jsonText, std::string& error);

private:
    CommunityIndex() = default;
    bool parse(const std::string& jsonText);

    std::vector<CommunityIndexEntry> mEntries;
    bool mLoaded = false;
    std::string mLastError;
    std::chrono::system_clock::time_point mLastUpdated{};

    std::string mIndexUrl =
        "https://raw.githubusercontent.com/bonneballefx/"
        "bbfx-community-plugins/main/index.json";
    int mCacheTtlSeconds = 3600;
};

} // namespace bbfx
