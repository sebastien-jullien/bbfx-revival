#include "Sha256.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace bbfx {

// FIPS 180-4 SHA-256 round constants
static const uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

Sha256::Sha256() {
    mState[0] = 0x6a09e667; mState[1] = 0xbb67ae85; mState[2] = 0x3c6ef372; mState[3] = 0xa54ff53a;
    mState[4] = 0x510e527f; mState[5] = 0x9b05688c; mState[6] = 0x1f83d9ab; mState[7] = 0x5be0cd19;
    std::memset(mBuffer, 0, sizeof(mBuffer));
}

void Sha256::transform(const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t(block[i * 4 + 0]) << 24)
             | (uint32_t(block[i * 4 + 1]) << 16)
             | (uint32_t(block[i * 4 + 2]) << 8)
             | (uint32_t(block[i * 4 + 3]) << 0);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = mState[0], b = mState[1], c = mState[2], d = mState[3];
    uint32_t e = mState[4], f = mState[5], g = mState[6], h = mState[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + kK[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    mState[0] += a; mState[1] += b; mState[2] += c; mState[3] += d;
    mState[4] += e; mState[5] += f; mState[6] += g; mState[7] += h;
}

void Sha256::update(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    while (len > 0) {
        size_t toCopy = std::min<size_t>(64 - mBufferLen, len);
        std::memcpy(mBuffer + mBufferLen, p, toCopy);
        mBufferLen += toCopy;
        p += toCopy;
        len -= toCopy;
        mBitLen += toCopy * 8;
        if (mBufferLen == 64) {
            transform(mBuffer);
            mBufferLen = 0;
        }
    }
}

std::string Sha256::finalizeHex() {
    // Pad
    mBuffer[mBufferLen++] = 0x80;
    if (mBufferLen > 56) {
        std::memset(mBuffer + mBufferLen, 0, 64 - mBufferLen);
        transform(mBuffer);
        mBufferLen = 0;
    }
    std::memset(mBuffer + mBufferLen, 0, 56 - mBufferLen);
    // Append bit length (big endian)
    for (int i = 0; i < 8; ++i) {
        mBuffer[56 + i] = static_cast<uint8_t>((mBitLen >> (56 - i * 8)) & 0xff);
    }
    transform(mBuffer);

    char out[65];
    for (int i = 0; i < 8; ++i) {
        std::snprintf(out + i * 8, 9, "%08x", mState[i]);
    }
    out[64] = 0;
    return std::string(out);
}

std::string Sha256::hashFile(const std::string& filepath) {
    std::ifstream f(filepath, std::ios::binary);
    if (!f.is_open()) return std::string();
    Sha256 h;
    constexpr size_t kChunk = 64 * 1024;
    std::vector<char> buf(kChunk);
    while (f) {
        f.read(buf.data(), kChunk);
        std::streamsize got = f.gcount();
        if (got > 0) h.update(buf.data(), static_cast<size_t>(got));
    }
    return h.finalizeHex();
}

std::string Sha256::hashBuffer(const void* data, size_t len) {
    Sha256 h;
    h.update(data, len);
    return h.finalizeHex();
}

} // namespace bbfx
