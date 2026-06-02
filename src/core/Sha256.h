#pragma once

#include <cstdint>
#include <string>

namespace bbfx {

/// Self-contained SHA-256 implementation (RFC 6234).
/// Pure C++ — no OpenSSL dependency, suitable for content-addressed
/// asset cache identification (AssetManifest v3.5.2+).
///
/// Performance ~200 MB/s on a modern x86_64 — sufficient for cache
/// indexing at project save time.
class Sha256 {
public:
    Sha256();

    /// Append `len` bytes from `data` to the running hash.
    void update(const void* data, size_t len);

    /// Finalize and return the 64-char lowercase hex digest.
    std::string finalizeHex();

    /// One-shot helpers.
    static std::string hashFile(const std::string& filepath);
    static std::string hashBuffer(const void* data, size_t len);

private:
    uint32_t mState[8];
    uint64_t mBitLen = 0;
    uint8_t  mBuffer[64];
    size_t   mBufferLen = 0;

    void transform(const uint8_t block[64]);
};

} // namespace bbfx
