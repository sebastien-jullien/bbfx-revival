#pragma once

#include <functional>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace bbfx {

/// v3.5 Lot V — GitHub REST v3 wrapper for the plugin publish flow.
///
/// The full flow is :
///   1. beginDeviceFlow() — POST /login/device/code, get user_code + url
///   2. pollDeviceFlow()  — GET  /login/oauth/access_token, repeat until
///                          user authorises in browser
///   3. storeToken()      — SettingsManager::save() with XOR-scrambled
///                          payload so the raw token is never on disk
///   4. fork()            — POST /repos/<org>/<repo>/forks
///   5. ensureBranch()    — POST /repos/<user>/<repo>/git/refs
///   6. commitFile()      — PUT  /repos/<user>/<repo>/contents/<path>
///   7. openPullRequest() — POST /repos/<org>/<repo>/pulls
///
/// Everything uses the existing HttpClient — no new network deps.
class GitHubPublisher {
public:
    /// BBFx public OAuth Device Flow client id. The user authorises a
    /// read/write scope on their account; no server-side secret needed.
    /// In production a real client id would be shipped here.
    static constexpr const char* kClientId = "BBFx-Studio-Device-Flow";

    /// Upstream community repo that BBFx plugins publish against.
    static constexpr const char* kUpstreamOwner = "bonneballefx";
    static constexpr const char* kUpstreamRepo  = "bbfx-community-plugins";

    struct DeviceCode {
        std::string deviceCode;
        std::string userCode;
        std::string verificationUri;
        int         interval = 5;        // poll seconds
        int         expiresIn = 900;     // seconds
        std::string error;               // non-empty on failure
    };

    struct TokenResult {
        std::string token;               // OAuth access token on success
        std::string error;               // "pending" / "slow_down" / other
        bool        pending = false;     // true if user not yet approved
    };

    GitHubPublisher() = default;

    /// Begin an OAuth device flow. Returns user_code + verification URL
    /// so the UI can show them to the user.
    DeviceCode beginDeviceFlow();

    /// Poll for the access token. Call every `code.interval` seconds
    /// until `result.pending` flips to false.
    TokenResult pollDeviceFlow(const DeviceCode& code);

    /// Persist (scrambled) + cache the access token for future calls.
    void storeToken(const std::string& token, const std::string& login = "");

    /// Restore the access token from settings (handles the scramble).
    /// Empty string if not authenticated.
    std::string storedToken() const;

    /// Currently authenticated login, or empty if no token.
    std::string storedLogin() const;

    bool isAuthenticated() const { return !storedToken().empty(); }

    /// GET /user — validates the token and returns the login.
    std::optional<std::string> whoami();

    /// POST /repos/<upstreamOwner>/<upstreamRepo>/forks. Returns true
    /// iff the fork now exists under `storedLogin()`.
    bool forkUpstream();

    /// POST /repos/<user>/<repo>/git/refs with a new branch pointed at
    /// `fromSha`. Returns true iff the branch was created (or already
    /// present).
    bool ensureBranch(const std::string& branch, const std::string& fromSha);

    /// PUT /repos/<user>/<repo>/contents/<path> with `content` body
    /// (base64-encoded server-side; we pass raw content + the API takes
    /// care of encoding). `prevSha` is optional — when set the PUT
    /// updates rather than creates.
    bool commitFile(const std::string& branch,
                      const std::string& path,
                      const std::string& content,
                      const std::string& message,
                      std::optional<std::string> prevSha = std::nullopt);

    /// POST /repos/<upstreamOwner>/<upstreamRepo>/pulls — opens the PR
    /// from the user's fork. Returns the PR URL on success.
    std::optional<std::string> openPullRequest(const std::string& branch,
                                                    const std::string& title,
                                                    const std::string& body);

    /// Scramble/unscramble helpers — public for tests.
    static std::string encodeToken(const std::string& raw);
    static std::string decodeToken(const std::string& scrambled);

private:
    // JSON-body helper.
    nlohmann::json post(const std::string& url,
                          const nlohmann::json& body,
                          bool withAuth);
    nlohmann::json get (const std::string& url, bool withAuth);
    nlohmann::json put (const std::string& url, const nlohmann::json& body);
};

} // namespace bbfx
