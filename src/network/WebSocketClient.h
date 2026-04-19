#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace bbfx {

/// Async WebSocket client.
///
/// Lot E scope:
///  - ws://  (plain)      : supported
///  - wss:// (TLS)        : supported when websocketpp's TLS transport is
///                          present (built with websocketpp[tls] in vcpkg
///                          and linking OpenSSL). The current build ships
///                          without TLS transport to keep dependencies
///                          minimal; wss:// connect() then returns a
///                          Connection in `Error` state with a descriptive
///                          `lastError()`. Lot F can flip the feature on
///                          by adding `openssl` to vcpkg.json and rebuilding.
///
/// Callbacks fire on a dedicated IO thread. The engine pumps them onto the
/// main thread via `HttpClient::pumpMainThread()`'s queue (shared main
/// queue — see connect()). For now we prefer the simplicity of having each
/// callback be responsible for thread-safety in user code.
class WebSocketConnection;

struct WebSocketCallbacks {
    std::function<void()>                                 onOpen;
    std::function<void(const std::string&, bool binary)>  onMessage;
    std::function<void(int code, const std::string&)>     onClose;
    std::function<void(const std::string&)>               onError;
};

class WebSocketClient {
public:
    static WebSocketClient& instance();

    /// Connect to the given ws:// or wss:// URL. Returns a Connection handle
    /// immediately; messages arrive through the callbacks. If the scheme is
    /// unsupported in this build (wss without TLS), the returned Connection
    /// is non-null but `isSupported()==false` and `lastError()` explains.
    std::shared_ptr<WebSocketConnection> connect(const std::string& url,
                                                 WebSocketCallbacks cbs);

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

private:
    WebSocketClient() = default;
};

/// Opaque handle for a live WebSocket connection.
class WebSocketConnection {
public:
    virtual ~WebSocketConnection() = default;
    virtual bool isSupported() const = 0;  // false if wss:// without TLS
    virtual bool send(const std::string& text) = 0;
    virtual bool sendBinary(const std::string& data) = 0;
    virtual void close(int code = 1000, const std::string& reason = "") = 0;
    virtual const std::string& lastError() const = 0;
};

} // namespace bbfx
