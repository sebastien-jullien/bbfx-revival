#include "plugin/GithubReactionsFetcher.h"

#include <chrono>

#include <nlohmann/json.hpp>

#include "network/HttpClient.h"

namespace bbfx {

GithubReactionsFetcher& GithubReactionsFetcher::instance() {
    static GithubReactionsFetcher inst;
    return inst;
}

void GithubReactionsFetcher::setRepo(const std::string& ownerSlashRepo) {
    std::lock_guard<std::mutex> lk(mMtx);
    mOwnerSlashRepo = ownerSlashRepo;
}

GithubReactionsFetcher::Result
GithubReactionsFetcher::cached(int issueNumber) const {
    std::lock_guard<std::mutex> lk(mMtx);
    auto it = mCache.find(issueNumber);
    if (it == mCache.end()) return Result{};
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - it->second.at).count();
    if (age > mCacheTtlSec) return Result{};
    return it->second.r;
}

void GithubReactionsFetcher::injectForTests(int issueNumber, Result r) {
    std::lock_guard<std::mutex> lk(mMtx);
    mCache[issueNumber] = { r, std::chrono::steady_clock::now() };
}

void GithubReactionsFetcher::fetch(int issueNumber,
                                    std::function<void(Result)> onComplete) {
    Result cached = this->cached(issueNumber);
    if (cached.valid) {
        if (onComplete) onComplete(cached);
        return;
    }

    std::string url;
    {
        std::lock_guard<std::mutex> lk(mMtx);
        url = "https://api.github.com/repos/" + mOwnerSlashRepo +
              "/issues/" + std::to_string(issueNumber) + "/reactions";
    }

    HttpClient::instance().get(url, [this, issueNumber, onComplete]
                                     (HttpResponse resp) mutable {
        Result r;
        if (resp.status == 200 && resp.error.empty()) {
            try {
                auto j = nlohmann::json::parse(resp.body);
                if (j.is_array()) {
                    for (const auto& e : j) {
                        if (!e.is_object() || !e.contains("content")) continue;
                        std::string content = e["content"].get<std::string>();
                        if      (content == "+1")     r.thumbsUp++;
                        else if (content == "-1")     r.thumbsDown++;
                        // Other reactions (heart, hooray, rocket, eyes,
                        // laugh, confused) contribute nothing — we're
                        // strictly up/down for the 0..5 rating.
                    }
                    int total = r.thumbsUp + r.thumbsDown;
                    r.rating = total == 0 ? 0.0f
                             : (float(r.thumbsUp) / total) * 5.0f;
                    r.valid  = true;
                }
            } catch (const std::exception&) {
                // malformed body — leave r.valid=false
            }
        }
        {
            std::lock_guard<std::mutex> lk(mMtx);
            mCache[issueNumber] = { r, std::chrono::steady_clock::now() };
        }
        if (onComplete) onComplete(r);
    }, 15);
}

} // namespace bbfx
