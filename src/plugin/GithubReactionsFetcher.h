#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace bbfx {

/// v3.5 Lot I — community rating signal via GitHub Reactions API.
///
/// Each community plugin is represented by a GitHub issue in the
/// `bonneballefx/bbfx-community-plugins` repo. Users upvote/downvote via
/// standard GitHub reactions (👍 / 👎). We aggregate those reactions and
/// expose a 0..5 rating that the CommunityBrowserPanel can overlay on top
/// of the index-provided rating (which is typically the CI-computed
/// snapshot at last release).
///
/// The fetcher caches results in-process for 1h by default (same TTL as
/// the index) to avoid hammering the unauthenticated GitHub API (60 req/h
/// limit). It never uses a token to keep Lot I fully read-only and
/// zero-config.
class GithubReactionsFetcher {
public:
    struct Result {
        int   thumbsUp   = 0;
        int   thumbsDown = 0;
        float rating     = 0.0f;   // (up / (up + down)) * 5, 0 when up+down==0
        bool  valid      = false;
    };

    static GithubReactionsFetcher& instance();

    // Configure the repo (default: bonneballefx/bbfx-community-plugins).
    void setRepo(const std::string& ownerSlashRepo);

    // TTL in seconds; calls inside the TTL return the cached result.
    void setCacheTtlSeconds(int seconds) { mCacheTtlSec = seconds; }

    // Start an async fetch for a given issue number. `onComplete` runs on
    // the main thread (HttpClient pump). If a fresh cached result exists
    // it is returned synchronously via the callback before returning.
    void fetch(int issueNumber, std::function<void(Result)> onComplete);

    // Lookup the cache synchronously. Returns {valid=false} if not cached.
    Result cached(int issueNumber) const;

    // Test helper: inject a Result for a given issue number (bypass network).
    void injectForTests(int issueNumber, Result r);

private:
    GithubReactionsFetcher() = default;

    struct CacheEntry {
        Result r;
        std::chrono::steady_clock::time_point at;
    };

    mutable std::mutex mMtx;
    std::map<int, CacheEntry> mCache;
    std::string mOwnerSlashRepo = "bonneballefx/bbfx-community-plugins";
    int mCacheTtlSec = 3600;
};

} // namespace bbfx
