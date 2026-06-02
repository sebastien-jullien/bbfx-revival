#include "network/HttpClient.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <curl/curl.h>

// ── SHA-256 implementation (self-contained, public domain) ─────────────────
// Small portable implementation; we don't link OpenSSL just for file hashing.
namespace {

struct Sha256Ctx {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t buf[64];
    uint32_t buflen;
};

static constexpr uint32_t kSha256K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

void sha256Init(Sha256Ctx& c) {
    c.bitlen = 0; c.buflen = 0;
    const uint32_t h0[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    for (int i = 0; i < 8; ++i) c.state[i] = h0[i];
}

void sha256Transform(Sha256Ctx& c, const uint8_t* data) {
    uint32_t m[64];
    for (int i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = (uint32_t(data[j]) << 24) | (uint32_t(data[j+1]) << 16) |
               (uint32_t(data[j+2]) << 8) | uint32_t(data[j+3]);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(m[i-15],7) ^ rotr(m[i-15],18) ^ (m[i-15] >> 3);
        uint32_t s1 = rotr(m[i-2],17) ^ rotr(m[i-2],19) ^ (m[i-2] >> 10);
        m[i] = m[i-16] + s0 + m[i-7] + s1;
    }
    uint32_t a=c.state[0],b=c.state[1],cc=c.state[2],d=c.state[3],
             e=c.state[4],f=c.state[5],g=c.state[6],h=c.state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + kSha256K[i] + m[i];
        uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
        uint32_t mj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c.state[0]+=a; c.state[1]+=b; c.state[2]+=cc; c.state[3]+=d;
    c.state[4]+=e; c.state[5]+=f; c.state[6]+=g; c.state[7]+=h;
}

void sha256Update(Sha256Ctx& c, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        c.buf[c.buflen++] = data[i];
        if (c.buflen == 64) {
            sha256Transform(c, c.buf);
            c.bitlen += 512;
            c.buflen = 0;
        }
    }
}

std::string sha256Final(Sha256Ctx& c) {
    uint32_t i = c.buflen;
    if (c.buflen < 56) {
        c.buf[i++] = 0x80;
        while (i < 56) c.buf[i++] = 0;
    } else {
        c.buf[i++] = 0x80;
        while (i < 64) c.buf[i++] = 0;
        sha256Transform(c, c.buf);
        std::memset(c.buf, 0, 56);
    }
    c.bitlen += uint64_t(c.buflen) * 8;
    c.buf[63] = uint8_t(c.bitlen);
    c.buf[62] = uint8_t(c.bitlen >> 8);
    c.buf[61] = uint8_t(c.bitlen >> 16);
    c.buf[60] = uint8_t(c.bitlen >> 24);
    c.buf[59] = uint8_t(c.bitlen >> 32);
    c.buf[58] = uint8_t(c.bitlen >> 40);
    c.buf[57] = uint8_t(c.bitlen >> 48);
    c.buf[56] = uint8_t(c.bitlen >> 56);
    sha256Transform(c, c.buf);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int j = 0; j < 8; ++j) oss << std::setw(8) << c.state[j];
    return oss.str();
}

} // anonymous SHA-256

namespace bbfx {

std::string HttpClient::sha256File(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return {};
    Sha256Ctx c;
    sha256Init(c);
    std::vector<uint8_t> buf(1 << 16);
    while (f.good()) {
        f.read(reinterpret_cast<char*>(buf.data()), buf.size());
        auto n = f.gcount();
        if (n > 0) sha256Update(c, buf.data(), size_t(n));
    }
    return sha256Final(c);
}

// ── Curl helpers ───────────────────────────────────────────────────────────

static size_t writeToString(char* ptr, size_t size, size_t nmemb, void* ud) {
    auto* out = static_cast<std::string*>(ud);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

static size_t writeToFile(char* ptr, size_t size, size_t nmemb, void* ud) {
    auto* f = static_cast<std::ofstream*>(ud);
    f->write(ptr, static_cast<std::streamsize>(size * nmemb));
    return f->good() ? size * nmemb : 0;
}

static size_t headerCb(char* ptr, size_t size, size_t nmemb, void* ud) {
    auto* map = static_cast<std::map<std::string, std::string>*>(ud);
    size_t total = size * nmemb;
    std::string line(ptr, total);
    auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        // trim CRLF and leading whitespace
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' '))
            value.pop_back();
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
            value.erase(value.begin());
        (*map)[key] = value;
    }
    return total;
}

// La progression + la complétion sont transmises au thread principal via une
// std::function stockée dans ProgressLink (ci-dessous) ; progressCbLinked pousse
// un lambda dans la file principale de HttpClient via les pointeurs mainMtx/
// mainQueue. (Un ancien progressCb/ProgressState « placeholder » a été retiré :
// code mort qui faisait un reinterpret_cast<std::mutex*> d'un HttpClient* = UB.)

namespace detail {
struct ProgressLink {
    HttpClient* client;
    std::function<void(size_t, size_t)> onProgress;
    std::mutex* mainMtx;
    std::queue<std::function<void()>>* mainQueue;
    std::atomic<size_t> lastReported{0};
};
} // namespace detail

static int progressCbLinked(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                             curl_off_t /*ult*/, curl_off_t /*uln*/) {
    auto* link = static_cast<detail::ProgressLink*>(clientp);
    if (!link || !link->onProgress) return 0;
    size_t d = dlnow < 0 ? 0 : size_t(dlnow);
    size_t t = dltotal < 0 ? 0 : size_t(dltotal);
    // Throttle: only report every ~256 KB to avoid flooding the main queue.
    size_t lastReported = link->lastReported.load(std::memory_order_relaxed);
    if (d - lastReported < (256 * 1024) && (t == 0 || d < t)) return 0;
    link->lastReported.store(d, std::memory_order_relaxed);

    auto cb = link->onProgress;
    auto* mtx = link->mainMtx;
    auto* q = link->mainQueue;
    if (mtx && q) {
        std::lock_guard<std::mutex> lk(*mtx);
        q->push([cb, d, t]() { cb(d, t); });
    }
    return 0;
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

HttpClient& HttpClient::instance() {
    static HttpClient inst;
    return inst;
}

HttpClient::HttpClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    mRunning = true;
    mWorker = std::thread([this] { workerLoop(); });
}

HttpClient::~HttpClient() {
    mRunning = false;
    mWorkerCv.notify_all();
    if (mWorker.joinable()) mWorker.join();
    curl_global_cleanup();
}

void HttpClient::workerLoop() {
    while (mRunning) {
        WorkItem item;
        {
            std::unique_lock<std::mutex> lk(mWorkerMtx);
            mWorkerCv.wait(lk, [this] {
                return !mWorkerQueue.empty() || !mRunning;
            });
            if (!mRunning && mWorkerQueue.empty()) return;
            item = std::move(mWorkerQueue.front());
            mWorkerQueue.pop();
        }
        try {
            item.run();
        } catch (const std::exception& e) {
            std::cerr << "[HttpClient] worker exception: " << e.what() << std::endl;
        }
        mInFlight.fetch_sub(1, std::memory_order_acq_rel);
    }
}

void HttpClient::pumpMainThread() {
    std::queue<std::function<void()>> local;
    {
        std::lock_guard<std::mutex> lk(mMainMtx);
        std::swap(local, mMainQueue);
    }
    while (!local.empty()) {
        try { local.front()(); } catch (const std::exception& e) {
            std::cerr << "[HttpClient] main callback exception: " << e.what() << std::endl;
        }
        local.pop();
    }
}

bool HttpClient::waitIdle(int timeoutSeconds) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
    while (mInFlight.load(std::memory_order_acquire) > 0 ||
           [this]{ std::lock_guard<std::mutex> lk(mMainMtx); return !mMainQueue.empty(); }()) {
        pumpMainThread();
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    pumpMainThread();
    return true;
}

// ── Transports ─────────────────────────────────────────────────────────────

static HttpResponse doGetSync(const std::string& url, int timeoutSeconds) {
    HttpResponse r;
    CURL* c = curl_easy_init();
    if (!c) { r.error = "curl_easy_init failed"; return r; }
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, long(timeoutSeconds));
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(c, CURLOPT_WRITEDATA,     &r.body);
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(c, CURLOPT_HEADERDATA,     &r.headers);
    curl_easy_setopt(c, CURLOPT_USERAGENT,      "bbfx/3.5.0");
    CURLcode rc = curl_easy_perform(c);
    if (rc != CURLE_OK) r.error = curl_easy_strerror(rc);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    r.status = int(status);
    r.bytes = r.body.size();
    curl_easy_cleanup(c);
    return r;
}

HttpResponse HttpClient::getSync(const std::string& url, int timeoutSeconds) {
    return doGetSync(url, timeoutSeconds);
}

static HttpResponse doMethodBodySync(const std::string& method,
                                          const std::string& url,
                                          const std::string& body,
                                          const std::map<std::string, std::string>& headers,
                                          int timeoutSeconds) {
    HttpResponse r;
    CURL* c = curl_easy_init();
    if (!c) { r.error = "curl_easy_init failed"; return r; }
    curl_slist* hdrs = nullptr;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    if (method == "POST") {
        curl_easy_setopt(c, CURLOPT_POST, 1L);
    } else {
        curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, method.c_str());
    }
    curl_easy_setopt(c, CURLOPT_POSTFIELDS,    body.c_str());
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE_LARGE, curl_off_t(body.size()));
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT,        long(timeoutSeconds));
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION,  writeToString);
    curl_easy_setopt(c, CURLOPT_WRITEDATA,      &r.body);
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(c, CURLOPT_HEADERDATA,     &r.headers);
    curl_easy_setopt(c, CURLOPT_USERAGENT,      "bbfx/3.5.0");
    for (const auto& [k, v] : headers) {
        std::string h = k + ": " + v;
        hdrs = curl_slist_append(hdrs, h.c_str());
    }
    if (hdrs) curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    CURLcode rc = curl_easy_perform(c);
    if (rc != CURLE_OK) r.error = curl_easy_strerror(rc);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    r.status = int(status);
    r.bytes = r.body.size();
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(c);
    return r;
}

HttpResponse HttpClient::postSync(const std::string& url,
                                       const std::string& body,
                                       const std::map<std::string, std::string>& headers,
                                       int timeoutSeconds) {
    return doMethodBodySync("POST", url, body, headers, timeoutSeconds);
}

HttpResponse HttpClient::putSync(const std::string& url,
                                      const std::string& body,
                                      const std::map<std::string, std::string>& headers,
                                      int timeoutSeconds) {
    return doMethodBodySync("PUT", url, body, headers, timeoutSeconds);
}

void HttpClient::get(const std::string& url,
                     std::function<void(HttpResponse)> cb,
                     int timeoutSeconds) {
    mInFlight.fetch_add(1, std::memory_order_acq_rel);
    WorkItem item;
    item.run = [this, url, cb, timeoutSeconds]() {
        HttpResponse r = doGetSync(url, timeoutSeconds);
        std::lock_guard<std::mutex> lk(mMainMtx);
        mMainQueue.push([cb, r]() { cb(r); });
    };
    {
        std::lock_guard<std::mutex> lk(mWorkerMtx);
        mWorkerQueue.push(std::move(item));
    }
    mWorkerCv.notify_one();
}

void HttpClient::post(const std::string& url,
                      const std::string& body,
                      const std::map<std::string, std::string>& headers,
                      std::function<void(HttpResponse)> cb,
                      int timeoutSeconds) {
    mInFlight.fetch_add(1, std::memory_order_acq_rel);
    WorkItem item;
    item.run = [this, url, body, headers, cb, timeoutSeconds]() {
        HttpResponse r;
        CURL* c = curl_easy_init();
        if (!c) { r.error = "curl_easy_init failed"; }
        curl_slist* hdrs = nullptr;
        if (c) {
            curl_easy_setopt(c, CURLOPT_URL, url.c_str());
            curl_easy_setopt(c, CURLOPT_POST, 1L);
            curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE_LARGE, curl_off_t(body.size()));
            curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(c, CURLOPT_TIMEOUT, long(timeoutSeconds));
            curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeToString);
            curl_easy_setopt(c, CURLOPT_WRITEDATA,     &r.body);
            curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, headerCb);
            curl_easy_setopt(c, CURLOPT_HEADERDATA,     &r.headers);
            curl_easy_setopt(c, CURLOPT_USERAGENT,      "bbfx/3.5.0");
            for (const auto& [k, v] : headers) {
                std::string h = k + ": " + v;
                hdrs = curl_slist_append(hdrs, h.c_str());
            }
            if (hdrs) curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
            CURLcode rc = curl_easy_perform(c);
            if (rc != CURLE_OK) r.error = curl_easy_strerror(rc);
            long status = 0;
            curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
            r.status = int(status);
            r.bytes = r.body.size();
            curl_slist_free_all(hdrs);
            curl_easy_cleanup(c);
        }
        std::lock_guard<std::mutex> lk(mMainMtx);
        mMainQueue.push([cb, r]() { cb(r); });
    };
    {
        std::lock_guard<std::mutex> lk(mWorkerMtx);
        mWorkerQueue.push(std::move(item));
    }
    mWorkerCv.notify_one();
}

void HttpClient::download(HttpDownloadRequest req) {
    mInFlight.fetch_add(1, std::memory_order_acq_rel);
    WorkItem item;
    HttpClient* self = this;
    item.run = [self, req]() {
        std::string error;
        bool ok = false;
        {
            std::ofstream f(req.destPath, std::ios::binary);
            if (!f.is_open()) {
                error = "cannot open destination for write: " + req.destPath;
            } else {
                detail::ProgressLink link;
                link.client = self;
                link.onProgress = req.onProgress;
                link.mainMtx = &self->mMainMtx;
                link.mainQueue = &self->mMainQueue;

                CURL* c = curl_easy_init();
                if (!c) {
                    error = "curl_easy_init failed";
                } else {
                    curl_easy_setopt(c, CURLOPT_URL, req.url.c_str());
                    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
                    curl_easy_setopt(c, CURLOPT_TIMEOUT, long(req.timeoutSeconds));
                    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
                    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeToFile);
                    curl_easy_setopt(c, CURLOPT_WRITEDATA,     &f);
                    curl_easy_setopt(c, CURLOPT_NOPROGRESS, req.onProgress ? 0L : 1L);
                    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, progressCbLinked);
                    curl_easy_setopt(c, CURLOPT_XFERINFODATA, &link);
                    curl_easy_setopt(c, CURLOPT_USERAGENT, "bbfx/3.5.0");
                    CURLcode rc = curl_easy_perform(c);
                    if (rc != CURLE_OK) {
                        error = curl_easy_strerror(rc);
                    } else {
                        ok = true;
                    }
                    curl_easy_cleanup(c);
                }
            }
        } // f closed

        if (ok && !req.expectedSha256.empty()) {
            // Comparaison insensible à la casse : un manifest peut fournir le hash
            // en MAJUSCULES, sha256File() le produit en minuscules → sinon faux négatif.
            auto toLower = [](std::string s) {
                for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                return s;
            };
            std::string got = sha256File(req.destPath);
            if (toLower(got) != toLower(req.expectedSha256)) {
                ok = false;
                error = "SHA-256 mismatch: got " + got + ", expected " + req.expectedSha256;
                std::remove(req.destPath.c_str());
            }
        }

        std::lock_guard<std::mutex> lk(self->mMainMtx);
        auto cb = req.onComplete;
        self->mMainQueue.push([cb, ok, path = req.destPath, error]() {
            if (cb) cb(ok, path, error);
        });
    };
    {
        std::lock_guard<std::mutex> lk(mWorkerMtx);
        mWorkerQueue.push(std::move(item));
    }
    mWorkerCv.notify_one();
}

} // namespace bbfx
