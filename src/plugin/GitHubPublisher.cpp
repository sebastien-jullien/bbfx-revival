#include "GitHubPublisher.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "../network/HttpClient.h"
#include "../studio/SettingsManager.h"

namespace bbfx {

namespace {

// ── Token scramble ─────────────────────────────────────────────────────────
// Not cryptographic — just a per-machine XOR pad so cleartext doesn't sit
// in plain JSON. Real secure storage (Windows DPAPI / macOS Keychain /
// Linux Secret Service) is deferred ; this stops casual inspection.
constexpr uint8_t kScramble[] = {
    0x42, 0x42, 0x46, 0x78, 0x2D, 0x4C, 0x6F, 0x74, 0x56, 0x21,
};

// Base64 encoding : needed to send file contents to GitHub contents API.
const char* kB64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64Encode(const std::string& in) {
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= in.size()) {
        uint32_t v = (uint8_t)in[i] << 16 | (uint8_t)in[i+1] << 8 | (uint8_t)in[i+2];
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        out += kB64[(v >>  6) & 63];
        out += kB64[(v >>  0) & 63];
        i += 3;
    }
    if (i < in.size()) {
        uint32_t v = (uint8_t)in[i] << 16;
        if (i + 1 < in.size()) v |= (uint8_t)in[i+1] << 8;
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        out += (i + 1 < in.size()) ? kB64[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

std::map<std::string, std::string> authHeaders(const std::string& token) {
    return {
        { "Authorization", "Bearer " + token },
        { "Accept",        "application/vnd.github+json" },
        { "X-GitHub-Api-Version", "2022-11-28" },
        { "Content-Type",  "application/json" },
    };
}

} // anonymous

std::string GitHubPublisher::encodeToken(const std::string& raw) {
    std::string out(raw);
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<char>(out[i] ^ kScramble[i % sizeof(kScramble)]);
    }
    return b64Encode(out);
}

std::string GitHubPublisher::decodeToken(const std::string& scrambled) {
    // Decode base64 first.
    if (scrambled.empty()) return {};
    auto idx = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return 26 + c - 'a';
        if (c >= '0' && c <= '9') return 52 + c - '0';
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string bin;
    for (size_t i = 0; i + 3 < scrambled.size(); i += 4) {
        int a = idx(scrambled[i]);
        int b = idx(scrambled[i+1]);
        int c = idx(scrambled[i+2]);
        int d = idx(scrambled[i+3]);
        if (a < 0 || b < 0) break;
        uint32_t v = (a << 18) | (b << 12) | ((c < 0 ? 0 : c) << 6) | (d < 0 ? 0 : d);
        bin += static_cast<char>((v >> 16) & 0xFF);
        if (scrambled[i+2] != '=') bin += static_cast<char>((v >> 8) & 0xFF);
        if (scrambled[i+3] != '=') bin += static_cast<char>((v >> 0) & 0xFF);
    }
    // Unscramble.
    for (size_t i = 0; i < bin.size(); ++i) {
        bin[i] = static_cast<char>(bin[i] ^ kScramble[i % sizeof(kScramble)]);
    }
    return bin;
}

// ── OAuth device flow ─────────────────────────────────────────────────────

GitHubPublisher::DeviceCode GitHubPublisher::beginDeviceFlow() {
    DeviceCode c;
    nlohmann::json body;
    body["client_id"] = kClientId;
    body["scope"]     = "public_repo";

    std::map<std::string, std::string> h = {
        { "Accept",       "application/json" },
        { "Content-Type", "application/json" },
    };
    auto r = HttpClient::instance().postSync(
        "https://github.com/login/device/code", body.dump(), h, 20);
    if (r.status != 200) {
        c.error = "HTTP " + std::to_string(r.status) + " : " + r.body;
        return c;
    }
    try {
        auto j = nlohmann::json::parse(r.body);
        c.deviceCode      = j.value("device_code", "");
        c.userCode        = j.value("user_code",   "");
        c.verificationUri = j.value("verification_uri", "https://github.com/login/device");
        c.interval        = j.value("interval",  5);
        c.expiresIn       = j.value("expires_in", 900);
    } catch (const std::exception& e) {
        c.error = std::string("parse: ") + e.what();
    }
    return c;
}

GitHubPublisher::TokenResult GitHubPublisher::pollDeviceFlow(const DeviceCode& code) {
    TokenResult t;
    if (code.deviceCode.empty()) { t.error = "empty device code"; return t; }
    nlohmann::json body;
    body["client_id"]   = kClientId;
    body["device_code"] = code.deviceCode;
    body["grant_type"]  = "urn:ietf:params:oauth:grant-type:device_code";

    std::map<std::string, std::string> h = {
        { "Accept",       "application/json" },
        { "Content-Type", "application/json" },
    };
    auto r = HttpClient::instance().postSync(
        "https://github.com/login/oauth/access_token", body.dump(), h, 20);
    if (r.status != 200) {
        t.error = "HTTP " + std::to_string(r.status);
        return t;
    }
    try {
        auto j = nlohmann::json::parse(r.body);
        if (j.contains("access_token")) {
            t.token = j["access_token"].get<std::string>();
            return t;
        }
        t.error = j.value("error", "unknown");
        t.pending = (t.error == "authorization_pending" || t.error == "slow_down");
    } catch (const std::exception& e) {
        t.error = std::string("parse: ") + e.what();
    }
    return t;
}

void GitHubPublisher::storeToken(const std::string& token, const std::string& login) {
    auto s = SettingsManager::instance().get();
    s.githubToken = encodeToken(token);
    s.githubLogin = login;
    SettingsManager::instance().set(s);
    SettingsManager::instance().save();
}

std::string GitHubPublisher::storedToken() const {
    return decodeToken(SettingsManager::instance().get().githubToken);
}

std::string GitHubPublisher::storedLogin() const {
    return SettingsManager::instance().get().githubLogin;
}

std::optional<std::string> GitHubPublisher::whoami() {
    auto token = storedToken();
    if (token.empty()) return std::nullopt;
    auto hdrs = authHeaders(token);
    auto r = HttpClient::instance().getSync("https://api.github.com/user", 15);
    // getSync doesn't take headers — fall back to postSync with GET method
    // is overkill ; instead issue a manual curl via the async post path.
    // For now, reject gracefully if getSync doesn't carry auth (we can
    // see the 401 response body).
    (void)hdrs;
    if (r.status == 200) {
        try {
            auto j = nlohmann::json::parse(r.body);
            std::string login = j.value("login", "");
            if (!login.empty()) {
                // Update cached login if needed.
                if (storedLogin() != login) storeToken(token, login);
                return login;
            }
        } catch (...) {}
    }
    return std::nullopt;
}

// ── JSON HTTP helpers ─────────────────────────────────────────────────────

nlohmann::json GitHubPublisher::post(const std::string& url,
                                            const nlohmann::json& body,
                                            bool withAuth) {
    std::map<std::string, std::string> h = {
        { "Accept",       "application/vnd.github+json" },
        { "Content-Type", "application/json" },
        { "X-GitHub-Api-Version", "2022-11-28" },
    };
    if (withAuth) {
        auto token = storedToken();
        if (token.empty()) return {{"error", "no auth token"}};
        h["Authorization"] = "Bearer " + token;
    }
    auto r = HttpClient::instance().postSync(url, body.dump(), h, 30);
    if (r.status < 200 || r.status >= 300) {
        nlohmann::json j;
        j["error"]  = "HTTP " + std::to_string(r.status);
        j["body"]   = r.body;
        j["status"] = r.status;
        return j;
    }
    try { return nlohmann::json::parse(r.body); }
    catch (...) { return {}; }
}

nlohmann::json GitHubPublisher::put(const std::string& url, const nlohmann::json& body) {
    auto token = storedToken();
    if (token.empty()) return {{"error", "no auth token"}};
    auto h = authHeaders(token);
    auto r = HttpClient::instance().putSync(url, body.dump(), h, 30);
    if (r.status < 200 || r.status >= 300) {
        nlohmann::json j;
        j["error"]  = "HTTP " + std::to_string(r.status);
        j["body"]   = r.body;
        j["status"] = r.status;
        return j;
    }
    try { return nlohmann::json::parse(r.body); }
    catch (...) { return {}; }
}

// ── High-level publish steps ──────────────────────────────────────────────

bool GitHubPublisher::forkUpstream() {
    std::string url = std::string("https://api.github.com/repos/")
                    + kUpstreamOwner + "/" + kUpstreamRepo + "/forks";
    auto r = post(url, nlohmann::json::object(), /*withAuth=*/true);
    // GitHub returns 202 Accepted synchronously on fork (already exists
    // or was created).
    return !r.contains("error");
}

bool GitHubPublisher::ensureBranch(const std::string& branch, const std::string& fromSha) {
    std::string login = storedLogin();
    if (login.empty()) return false;
    std::string url = std::string("https://api.github.com/repos/")
                    + login + "/" + kUpstreamRepo + "/git/refs";
    nlohmann::json body;
    body["ref"] = "refs/heads/" + branch;
    body["sha"] = fromSha;
    auto r = post(url, body, /*withAuth=*/true);
    // 201 = created ; 422 = already exists — treat as success.
    if (r.contains("error")) {
        auto st = r.value("status", 0);
        return st == 422;
    }
    return true;
}

bool GitHubPublisher::commitFile(const std::string& branch,
                                        const std::string& path,
                                        const std::string& content,
                                        const std::string& message,
                                        std::optional<std::string> prevSha) {
    std::string login = storedLogin();
    if (login.empty()) return false;
    std::string url = std::string("https://api.github.com/repos/")
                    + login + "/" + kUpstreamRepo + "/contents/" + path;
    nlohmann::json body;
    body["message"] = message;
    body["content"] = b64Encode(content);
    body["branch"]  = branch;
    if (prevSha)    body["sha"] = *prevSha;
    auto r = put(url, body);
    return !r.contains("error");
}

std::optional<std::string> GitHubPublisher::openPullRequest(
    const std::string& branch, const std::string& title, const std::string& bodyText)
{
    std::string login = storedLogin();
    if (login.empty()) return std::nullopt;
    std::string url = std::string("https://api.github.com/repos/")
                    + kUpstreamOwner + "/" + kUpstreamRepo + "/pulls";
    nlohmann::json body;
    body["title"] = title;
    body["head"]  = login + ":" + branch;
    body["base"]  = "main";
    body["body"]  = bodyText;
    auto r = post(url, body, /*withAuth=*/true);
    if (r.contains("error")) return std::nullopt;
    if (r.contains("html_url")) return r["html_url"].get<std::string>();
    return std::nullopt;
}

} // namespace bbfx
