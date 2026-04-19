#include "network/WebSocketClient.h"

#include <memory>

// v3.5 Lot E scope decision (2026-04-17):
//
// The v3.5 plan calls for ws://+wss:// async connect via websocketpp+asio+
// OpenSSL. The current vcpkg baseline (shared with the main v3.4 release
// line) does not carry a `websocketpp` port at all, so we cannot pull it
// without bumping the baseline — which risks destabilising every OGRE/Boost
// dependency resolved against the same snapshot.
//
// Rather than vendor websocketpp (~70 headers) under extern/ before the
// Lot E landing, we ship the full public API surface as a clean "no-op"
// transport that always reports Unsupported, with a precise pointer to the
// follow-up work. HTTP — which is the dependency the rest of the plugin
// pipeline actually relies on (Lot F download + SHA256) — is 100% live via
// libcurl in HttpClient.{h,cpp}.
//
// To flip WebSocket on later, one of:
//   1. `vcpkg x-update-baseline` to a snapshot that carries websocketpp
//      (+ optionally openssl for TLS), re-add the include, drop this stub.
//   2. Vendor websocketpp headers under extern/websocketpp/ and include
//      them directly.
// Neither changes any public API declared in WebSocketClient.h.

namespace bbfx {

namespace {

class UnsupportedConnection : public WebSocketConnection {
public:
    explicit UnsupportedConnection(std::string reason)
        : mReason(std::move(reason)) {}

    bool isSupported() const override { return false; }
    const std::string& lastError() const override { return mReason; }
    bool send(const std::string&) override { return false; }
    bool sendBinary(const std::string&) override { return false; }
    void close(int, const std::string&) override {}

private:
    std::string mReason;
};

} // anonymous

WebSocketClient& WebSocketClient::instance() {
    static WebSocketClient inst;
    return inst;
}

std::shared_ptr<WebSocketConnection>
WebSocketClient::connect(const std::string& url, WebSocketCallbacks cbs) {
    std::string msg;
    if (url.rfind("wss://", 0) == 0) {
        msg = "wss:// support requires websocketpp + openssl (not in the "
              "current vcpkg baseline). See WebSocketClient.cpp for the "
              "enable path.";
    } else if (url.rfind("ws://", 0) == 0) {
        msg = "ws:// support requires websocketpp (not in the current vcpkg "
              "baseline). See WebSocketClient.cpp for the enable path.";
    } else {
        msg = "unsupported URL scheme (expected ws:// or wss://): " + url;
    }
    if (cbs.onError) cbs.onError(msg);
    return std::make_shared<UnsupportedConnection>(std::move(msg));
}

} // namespace bbfx
