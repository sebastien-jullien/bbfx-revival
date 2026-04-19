#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace bbfx {

struct HttpResponse {
    int status = 0;          // HTTP status code (0 on transport error)
    std::string body;
    std::map<std::string, std::string> headers;
    std::string error;       // non-empty on any failure
    // bytes transferred (body length + headers, for diagnostics)
    size_t bytes = 0;
};

struct HttpDownloadRequest {
    std::string url;
    std::string destPath;
    std::string expectedSha256;    // optional; empty = no check
    int timeoutSeconds = 60;
    std::function<void(size_t downloaded, size_t total)> onProgress;   // may be null
    std::function<void(bool success, std::string path, std::string error)> onComplete;
};

/// Asynchronous HTTP client backed by libcurl.
///
/// - A single worker thread processes the request queue. Each curl_easy handle
///   is created per request so they stay independent.
/// - Callbacks are marshalled back to the main thread via `pumpMainThread()`,
///   which the engine calls every frame. This keeps Lua-visible behaviour
///   thread-safe even when callbacks reach back into sol::state.
/// - getSync() runs the same transport inline on the calling thread. Intended
///   for the CLI `--install` flow and for tests.
/// - Proxies: `HTTP_PROXY` / `HTTPS_PROXY` env vars are honoured by curl
///   directly (default behaviour); nothing else to wire.
class HttpClient {
public:
    static HttpClient& instance();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // Async variants — callback runs on the main thread after pumpMainThread().
    void get (const std::string& url,
              std::function<void(HttpResponse)> cb,
              int timeoutSeconds = 30);
    void post(const std::string& url,
              const std::string& body,
              const std::map<std::string, std::string>& headers,
              std::function<void(HttpResponse)> cb,
              int timeoutSeconds = 30);
    void download(HttpDownloadRequest req);

    // Synchronous; blocks the caller until the request completes or times
    // out. Use sparingly — never from the render thread.
    HttpResponse getSync(const std::string& url, int timeoutSeconds = 30);

    // v3.5 Lot V — Synchronous POST/PUT for the GitHub publishing flow.
    // Convenience wrappers around the async api + waitIdle; they let
    // the publisher sequence API calls linearly.
    HttpResponse postSync(const std::string& url,
                            const std::string& body,
                            const std::map<std::string, std::string>& headers,
                            int timeoutSeconds = 30);
    HttpResponse putSync (const std::string& url,
                            const std::string& body,
                            const std::map<std::string, std::string>& headers,
                            int timeoutSeconds = 30);

    // Called by the engine every frame. Runs queued main-thread callbacks.
    // Safe to call when nothing is in flight (no-op).
    void pumpMainThread();

    // Hash a file using SHA-256. Returns the lowercase hex digest, or an
    // empty string on error (file missing, IO error).
    static std::string sha256File(const std::string& path);

    // Tests may want to flush pending work deterministically. Blocks until
    // the worker queue is empty AND all main-thread callbacks have been
    // dispatched. Returns false if timed out.
    bool waitIdle(int timeoutSeconds = 30);

private:
    HttpClient();
    ~HttpClient();

    void workerLoop();

    struct WorkItem {
        std::function<void()> run;   // the curl call itself, on the worker
    };

    std::thread mWorker;
    std::atomic<bool> mRunning{false};
    std::atomic<int> mInFlight{0};   // items queued or being executed

    std::mutex mWorkerMtx;
    std::condition_variable mWorkerCv;
    std::queue<WorkItem> mWorkerQueue;

    std::mutex mMainMtx;
    std::queue<std::function<void()>> mMainQueue;
};

} // namespace bbfx
