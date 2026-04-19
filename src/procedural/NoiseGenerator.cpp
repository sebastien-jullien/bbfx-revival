#include "NoiseGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

#include <cstring>

#include <OgreHardwarePixelBuffer.h>
#include <OgrePixelFormat.h>
#include <OgreTexture.h>
#include <OgreTextureManager.h>

namespace bbfx {

namespace {

// ── 256-entry permutation built from an LCG seed ──────────────────────────
using Perm = std::array<uint8_t, 512>;

Perm makePermutation(int seed) {
    Perm p{};
    std::array<uint8_t, 256> base{};
    for (int i = 0; i < 256; ++i) base[i] = static_cast<uint8_t>(i);
    // Deterministic Fisher-Yates from `seed`.
    uint32_t s = static_cast<uint32_t>(seed) * 1664525u + 1013904223u;
    for (int i = 255; i > 0; --i) {
        s = s * 1664525u + 1013904223u;
        int j = static_cast<int>(s % static_cast<uint32_t>(i + 1));
        std::swap(base[i], base[j]);
    }
    for (int i = 0; i < 512; ++i) p[i] = base[i & 255];
    return p;
}

// Gradient lookups for simplex ----------------------------------------------
constexpr int grad3[12][3] = {
    {1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},
    {1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},
    {0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1}
};
constexpr int grad4[32][4] = {
    {0,1,1,1},{0,1,1,-1},{0,1,-1,1},{0,1,-1,-1},
    {0,-1,1,1},{0,-1,1,-1},{0,-1,-1,1},{0,-1,-1,-1},
    {1,0,1,1},{1,0,1,-1},{1,0,-1,1},{1,0,-1,-1},
    {-1,0,1,1},{-1,0,1,-1},{-1,0,-1,1},{-1,0,-1,-1},
    {1,1,0,1},{1,1,0,-1},{1,-1,0,1},{1,-1,0,-1},
    {-1,1,0,1},{-1,1,0,-1},{-1,-1,0,1},{-1,-1,0,-1},
    {1,1,1,0},{1,1,-1,0},{1,-1,1,0},{1,-1,-1,0},
    {-1,1,1,0},{-1,1,-1,0},{-1,-1,1,0},{-1,-1,-1,0}
};

inline float dot2(const int g[3], float x, float y) {
    return g[0] * x + g[1] * y;
}
inline float dot3(const int g[3], float x, float y, float z) {
    return g[0] * x + g[1] * y + g[2] * z;
}
inline float dot4(const int g[4], float x, float y, float z, float w) {
    return g[0] * x + g[1] * y + g[2] * z + g[3] * w;
}

// ── Simplex 2D (Ken Perlin's reference implementation) ────────────────────
float simplex2Core(float xin, float yin, const Perm& p) {
    const float F2 = 0.3660254037844386f;  // 0.5*(sqrt(3)-1)
    const float G2 = 0.21132486540518713f; // (3-sqrt(3))/6
    float s = (xin + yin) * F2;
    int i = static_cast<int>(std::floor(xin + s));
    int j = static_cast<int>(std::floor(yin + s));
    float t = (i + j) * G2;
    float X0 = i - t, Y0 = j - t;
    float x0 = xin - X0, y0 = yin - Y0;
    int i1, j1;
    if (x0 > y0) { i1 = 1; j1 = 0; }
    else         { i1 = 0; j1 = 1; }
    float x1 = x0 - i1 + G2;
    float y1 = y0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2;
    float y2 = y0 - 1.0f + 2.0f * G2;
    int ii = i & 255, jj = j & 255;
    int gi0 = p[ii + p[jj]] % 12;
    int gi1 = p[ii + i1 + p[jj + j1]] % 12;
    int gi2 = p[ii + 1 + p[jj + 1]] % 12;
    float n0 = 0, n1 = 0, n2 = 0;
    float t0 = 0.5f - x0 * x0 - y0 * y0;
    if (t0 > 0) { t0 *= t0; n0 = t0 * t0 * dot2(grad3[gi0], x0, y0); }
    float t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 > 0) { t1 *= t1; n1 = t1 * t1 * dot2(grad3[gi1], x1, y1); }
    float t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 > 0) { t2 *= t2; n2 = t2 * t2 * dot2(grad3[gi2], x2, y2); }
    return 70.0f * (n0 + n1 + n2);
}

// ── Simplex 3D ──────────────────────────────────────────────────────────
float simplex3Core(float xin, float yin, float zin, const Perm& p) {
    const float F3 = 1.0f / 3.0f;
    const float G3 = 1.0f / 6.0f;
    float s = (xin + yin + zin) * F3;
    int i = static_cast<int>(std::floor(xin + s));
    int j = static_cast<int>(std::floor(yin + s));
    int k = static_cast<int>(std::floor(zin + s));
    float t = (i + j + k) * G3;
    float X0 = i - t, Y0 = j - t, Z0 = k - t;
    float x0 = xin - X0, y0 = yin - Y0, z0 = zin - Z0;
    int i1, j1, k1, i2, j2, k2;
    if (x0 >= y0) {
        if      (y0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; }
        else if (x0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; }
        else               { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; }
    } else {
        if      (y0 < z0)  { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; }
        else if (x0 < z0)  { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; }
        else               { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; }
    }
    float x1 = x0 - i1 + G3;
    float y1 = y0 - j1 + G3;
    float z1 = z0 - k1 + G3;
    float x2 = x0 - i2 + 2.0f * G3;
    float y2 = y0 - j2 + 2.0f * G3;
    float z2 = z0 - k2 + 2.0f * G3;
    float x3 = x0 - 1.0f + 3.0f * G3;
    float y3 = y0 - 1.0f + 3.0f * G3;
    float z3 = z0 - 1.0f + 3.0f * G3;
    int ii = i & 255, jj = j & 255, kk = k & 255;
    int gi0 = p[ii + p[jj + p[kk]]] % 12;
    int gi1 = p[ii + i1 + p[jj + j1 + p[kk + k1]]] % 12;
    int gi2 = p[ii + i2 + p[jj + j2 + p[kk + k2]]] % 12;
    int gi3 = p[ii + 1 + p[jj + 1 + p[kk + 1]]] % 12;
    float n0 = 0, n1 = 0, n2 = 0, n3 = 0;
    float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0;
    if (t0 > 0) { t0 *= t0; n0 = t0*t0*dot3(grad3[gi0], x0, y0, z0); }
    float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1;
    if (t1 > 0) { t1 *= t1; n1 = t1*t1*dot3(grad3[gi1], x1, y1, z1); }
    float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2;
    if (t2 > 0) { t2 *= t2; n2 = t2*t2*dot3(grad3[gi2], x2, y2, z2); }
    float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3;
    if (t3 > 0) { t3 *= t3; n3 = t3*t3*dot3(grad3[gi3], x3, y3, z3); }
    return 32.0f * (n0 + n1 + n2 + n3);
}

// ── Simplex 4D ──────────────────────────────────────────────────────────
float simplex4Core(float xin, float yin, float zin, float win, const Perm& p) {
    const float F4 = (std::sqrt(5.0f) - 1.0f) / 4.0f;
    const float G4 = (5.0f - std::sqrt(5.0f)) / 20.0f;
    float s = (xin + yin + zin + win) * F4;
    int i = static_cast<int>(std::floor(xin + s));
    int j = static_cast<int>(std::floor(yin + s));
    int k = static_cast<int>(std::floor(zin + s));
    int l = static_cast<int>(std::floor(win + s));
    float t = (i + j + k + l) * G4;
    float X0 = i - t, Y0 = j - t, Z0 = k - t, W0 = l - t;
    float x0 = xin - X0, y0 = yin - Y0, z0 = zin - Z0, w0 = win - W0;
    // Rank the coordinates.
    int rankx = 0, ranky = 0, rankz = 0, rankw = 0;
    if (x0 > y0) rankx++; else ranky++;
    if (x0 > z0) rankx++; else rankz++;
    if (x0 > w0) rankx++; else rankw++;
    if (y0 > z0) ranky++; else rankz++;
    if (y0 > w0) ranky++; else rankw++;
    if (z0 > w0) rankz++; else rankw++;
    int i1 = rankx >= 3 ? 1 : 0;
    int j1 = ranky >= 3 ? 1 : 0;
    int k1 = rankz >= 3 ? 1 : 0;
    int l1 = rankw >= 3 ? 1 : 0;
    int i2 = rankx >= 2 ? 1 : 0;
    int j2 = ranky >= 2 ? 1 : 0;
    int k2 = rankz >= 2 ? 1 : 0;
    int l2 = rankw >= 2 ? 1 : 0;
    int i3 = rankx >= 1 ? 1 : 0;
    int j3 = ranky >= 1 ? 1 : 0;
    int k3 = rankz >= 1 ? 1 : 0;
    int l3 = rankw >= 1 ? 1 : 0;
    float x1 = x0 - i1 + G4, y1 = y0 - j1 + G4, z1 = z0 - k1 + G4, w1 = w0 - l1 + G4;
    float x2 = x0 - i2 + 2*G4, y2 = y0 - j2 + 2*G4, z2 = z0 - k2 + 2*G4, w2 = w0 - l2 + 2*G4;
    float x3 = x0 - i3 + 3*G4, y3 = y0 - j3 + 3*G4, z3 = z0 - k3 + 3*G4, w3 = w0 - l3 + 3*G4;
    float x4 = x0 - 1 + 4*G4, y4 = y0 - 1 + 4*G4, z4 = z0 - 1 + 4*G4, w4 = w0 - 1 + 4*G4;
    int ii = i & 255, jj = j & 255, kk = k & 255, ll = l & 255;
    int gi0 = p[ii + p[jj + p[kk + p[ll]]]] % 32;
    int gi1 = p[ii + i1 + p[jj + j1 + p[kk + k1 + p[ll + l1]]]] % 32;
    int gi2 = p[ii + i2 + p[jj + j2 + p[kk + k2 + p[ll + l2]]]] % 32;
    int gi3 = p[ii + i3 + p[jj + j3 + p[kk + k3 + p[ll + l3]]]] % 32;
    int gi4 = p[ii + 1 + p[jj + 1 + p[kk + 1 + p[ll + 1]]]] % 32;
    float n0=0, n1=0, n2=0, n3=0, n4=0;
    float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0 - w0*w0;
    if (t0 > 0) { t0 *= t0; n0 = t0*t0*dot4(grad4[gi0], x0, y0, z0, w0); }
    float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1 - w1*w1;
    if (t1 > 0) { t1 *= t1; n1 = t1*t1*dot4(grad4[gi1], x1, y1, z1, w1); }
    float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2 - w2*w2;
    if (t2 > 0) { t2 *= t2; n2 = t2*t2*dot4(grad4[gi2], x2, y2, z2, w2); }
    float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3 - w3*w3;
    if (t3 > 0) { t3 *= t3; n3 = t3*t3*dot4(grad4[gi3], x3, y3, z3, w3); }
    float t4 = 0.6f - x4*x4 - y4*y4 - z4*z4 - w4*w4;
    if (t4 > 0) { t4 *= t4; n4 = t4*t4*dot4(grad4[gi4], x4, y4, z4, w4); }
    return 27.0f * (n0 + n1 + n2 + n3 + n4);
}

// ── Worley (cellular F1) 2D/3D ───────────────────────────────────────────
inline float hash01(int x, int y, int z, int seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u
               + static_cast<uint32_t>(y) * 668265263u
               + static_cast<uint32_t>(z) * 1274126177u
               + static_cast<uint32_t>(seed);
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}
float worley2Core(float x, float y, int seed) {
    int cx = static_cast<int>(std::floor(x));
    int cy = static_cast<int>(std::floor(y));
    float fx = x - cx, fy = y - cy;
    float best = 2.0f;
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            float px = i + hash01(cx + i, cy + j, 0, seed);
            float py = j + hash01(cx + i, cy + j, 1, seed);
            float dx = px - fx, dy = py - fy;
            float d = dx*dx + dy*dy;
            if (d < best) best = d;
        }
    }
    return std::min(1.0f, std::sqrt(best));
}
float worley3Core(float x, float y, float z, int seed) {
    int cx = static_cast<int>(std::floor(x));
    int cy = static_cast<int>(std::floor(y));
    int cz = static_cast<int>(std::floor(z));
    float fx = x - cx, fy = y - cy, fz = z - cz;
    float best = 3.0f;
    for (int k = -1; k <= 1; ++k) {
        for (int j = -1; j <= 1; ++j) {
            for (int i = -1; i <= 1; ++i) {
                float px = i + hash01(cx + i, cy + j, cz + k, seed);
                float py = j + hash01(cx + i, cy + j, cz + k, seed + 101);
                float pz = k + hash01(cx + i, cy + j, cz + k, seed + 211);
                float dx = px - fx, dy = py - fy, dz = pz - fz;
                float d = dx*dx + dy*dy + dz*dz;
                if (d < best) best = d;
            }
        }
    }
    return std::min(1.0f, std::sqrt(best));
}

} // anonymous

// ── Public API ─────────────────────────────────────────────────────────────

float NoiseGenerator::simplex2D(float x, float y, int seed) {
    Perm p = makePermutation(seed);
    return simplex2Core(x, y, p);
}
float NoiseGenerator::simplex3D(float x, float y, float z, int seed) {
    Perm p = makePermutation(seed);
    return simplex3Core(x, y, z, p);
}
float NoiseGenerator::simplex4D(float x, float y, float z, float w, int seed) {
    Perm p = makePermutation(seed);
    return simplex4Core(x, y, z, w, p);
}
float NoiseGenerator::worley2D(float x, float y, int seed) {
    return worley2Core(x, y, seed);
}
float NoiseGenerator::worley3D(float x, float y, float z, int seed) {
    return worley3Core(x, y, z, seed);
}

void NoiseGenerator::curl2D(float x, float y, int seed, float& outX, float& outY) {
    // ∂ψ/∂y and -∂ψ/∂x approximated by finite differences on simplex2D.
    const float eps = 0.01f;
    Perm p = makePermutation(seed);
    float dy = (simplex2Core(x, y + eps, p) - simplex2Core(x, y - eps, p)) / (2.0f * eps);
    float dx = (simplex2Core(x + eps, y, p) - simplex2Core(x - eps, y, p)) / (2.0f * eps);
    outX =  dy;
    outY = -dx;
}

void NoiseGenerator::curl3D(float x, float y, float z, int seed,
                              float& outX, float& outY, float& outZ) {
    const float eps = 0.01f;
    Perm pA = makePermutation(seed);
    Perm pB = makePermutation(seed + 101);
    Perm pC = makePermutation(seed + 211);
    auto psiA = [&](float X, float Y, float Z) { return simplex3Core(X, Y, Z, pA); };
    auto psiB = [&](float X, float Y, float Z) { return simplex3Core(X, Y, Z, pB); };
    auto psiC = [&](float X, float Y, float Z) { return simplex3Core(X, Y, Z, pC); };
    // F = curl(ψ) where ψ = (ψA, ψB, ψC).
    float dCdy = (psiC(x, y + eps, z) - psiC(x, y - eps, z)) / (2.0f * eps);
    float dBdz = (psiB(x, y, z + eps) - psiB(x, y, z - eps)) / (2.0f * eps);
    float dAdz = (psiA(x, y, z + eps) - psiA(x, y, z - eps)) / (2.0f * eps);
    float dCdx = (psiC(x + eps, y, z) - psiC(x - eps, y, z)) / (2.0f * eps);
    float dBdx = (psiB(x + eps, y, z) - psiB(x - eps, y, z)) / (2.0f * eps);
    float dAdy = (psiA(x, y + eps, z) - psiA(x, y - eps, z)) / (2.0f * eps);
    outX = dCdy - dBdz;
    outY = dAdz - dCdx;
    outZ = dBdx - dAdy;
}

float NoiseGenerator::fbm2D(float x, float y, const FbmOptions& opts) {
    float amp = 1.0f, freq = 1.0f, sum = 0.0f, norm = 0.0f;
    Perm p = makePermutation(opts.seed);
    int octaves = std::max(1, opts.octaves);
    for (int i = 0; i < octaves; ++i) {
        sum  += amp * simplex2Core(x * freq, y * freq, p);
        norm += amp;
        amp  *= opts.gain;
        freq *= opts.lacunarity;
    }
    return sum / std::max(0.0001f, norm);
}

void NoiseGenerator::fillTexture2D(std::vector<float>& out, int w, int h,
                                     float scale, int seed) {
    out.assign(static_cast<size_t>(w) * h, 0.0f);
    Perm p = makePermutation(seed);
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            float x = (i + 0.5f) / w * scale;
            float y = (j + 0.5f) / h * scale;
            out[static_cast<size_t>(j) * w + i] = simplex2Core(x, y, p);
        }
    }
}

std::string NoiseGenerator::generateTexture(int w, int h, const std::string& kind,
                                                float scale, int octaves, int seed) {
    std::string lk;
    lk.reserve(kind.size());
    for (char c : kind) lk.push_back(static_cast<char>(std::tolower(c)));

    std::vector<uint8_t> data(static_cast<size_t>(w) * h);
    Perm p = makePermutation(seed);
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            float x = (i + 0.5f) / w * scale;
            float y = (j + 0.5f) / h * scale;
            float v = 0.0f;
            if      (lk == "worley")  v = 1.0f - worley2Core(x, y, seed);
            else if (lk == "fbm")     {
                FbmOptions o; o.seed = seed; o.octaves = std::max(1, octaves);
                v = fbm2D(x, y, o);
            }
            else                       v = simplex2Core(x, y, p);
            // Map [-1, 1] -> [0, 255]
            float n = std::clamp(v * 0.5f + 0.5f, 0.0f, 1.0f);
            data[static_cast<size_t>(j) * w + i] = static_cast<uint8_t>(n * 255.0f);
        }
    }

    try {
        std::string name = "bbfx_noise_" + kind + "_" + std::to_string(seed) +
                            "_" + std::to_string(w) + "x" + std::to_string(h);
        auto& tm = Ogre::TextureManager::getSingleton();
        if (auto existing = tm.getByName(name, "General"); existing) {
            tm.remove(existing);
        }
        auto tex = tm.createManual(name, "General",
                                    Ogre::TEX_TYPE_2D,
                                    w, h, 0, Ogre::PF_L8,
                                    Ogre::TU_DEFAULT);
        auto buf = tex->getBuffer();
        buf->lock(Ogre::HardwareBuffer::HBL_DISCARD);
        Ogre::PixelBox box = buf->getCurrentLock();
        std::memcpy(box.data, data.data(), data.size());
        buf->unlock();
        return name;
    } catch (const std::exception& e) {
        std::cerr << "[NoiseGenerator] generateTexture failed: " << e.what() << std::endl;
        return {};
    }
}

namespace {

void hsv2rgb(float h, float s, float v, float& r, float& g, float& b) {
    float c = v * s;
    float hp = h * 6.0f;
    float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    float m = v - c;
    float r1=0, g1=0, b1=0;
    if      (hp < 1) { r1=c; g1=x; }
    else if (hp < 2) { r1=x; g1=c; }
    else if (hp < 3) { g1=c; b1=x; }
    else if (hp < 4) { g1=x; b1=c; }
    else if (hp < 5) { r1=x; b1=c; }
    else             { r1=c; b1=x; }
    r = r1 + m; g = g1 + m; b = b1 + m;
}

void colorize(const std::string& scheme, float t, uint8_t& r, uint8_t& g, uint8_t& b) {
    t = std::clamp(t, 0.0f, 1.0f);
    float fr = 0, fg = 0, fb = 0;
    if (scheme == "fire") {
        fr = std::clamp(t * 3.0f, 0.0f, 1.0f);
        fg = std::clamp(t * 3.0f - 1.0f, 0.0f, 1.0f);
        fb = std::clamp(t * 3.0f - 2.0f, 0.0f, 1.0f);
    } else if (scheme == "ocean") {
        fr = t * 0.3f;
        fg = t * 0.6f;
        fb = 0.3f + t * 0.7f;
    } else if (scheme == "grayscale") {
        fr = fg = fb = t;
    } else {
        // rainbow default
        hsv2rgb(std::fmod(t, 1.0f), 1.0f, t > 0.01f ? 1.0f : 0.0f, fr, fg, fb);
    }
    r = static_cast<uint8_t>(std::round(fr * 255.0f));
    g = static_cast<uint8_t>(std::round(fg * 255.0f));
    b = static_cast<uint8_t>(std::round(fb * 255.0f));
}

std::string writeRGBATexture(const std::string& name, int w, int h,
                                 const std::vector<uint8_t>& rgba) {
    try {
        auto& tm = Ogre::TextureManager::getSingleton();
        if (auto existing = tm.getByName(name); existing) tm.remove(existing);
        auto tex = tm.createManual(name,
                     Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                     Ogre::TEX_TYPE_2D, w, h, 0,
                     Ogre::PF_A8R8G8B8, Ogre::TU_DEFAULT);
        auto buf = tex->getBuffer();
        buf->lock(Ogre::HardwareBuffer::HBL_DISCARD);
        auto box = buf->getCurrentLock();
        std::memcpy(box.data, rgba.data(), rgba.size());
        buf->unlock();
        return name;
    } catch (const std::exception& e) {
        std::cerr << "[NoiseGenerator] writeRGBATexture failed: " << e.what() << std::endl;
        return {};
    }
}

} // anonymous

std::string NoiseGenerator::mandelbrotTexture(int w, int h,
                                                     double centerX, double centerY,
                                                     double zoom, int maxIter,
                                                     const std::string& scheme) {
    if (w <= 0 || h <= 0 || zoom <= 0) return {};
    std::vector<uint8_t> data(static_cast<size_t>(w) * h * 4);
    double span = 4.0 / zoom;
    double minX = centerX - span * 0.5;
    double minY = centerY - span * 0.5 * h / w;
    double stepX = span / w;
    double stepY = span * h / w / h;
    for (int j = 0; j < h; ++j) {
        double ci = minY + j * stepY;
        for (int i = 0; i < w; ++i) {
            double cr = minX + i * stepX;
            double zr = 0, zi = 0; int iter = 0;
            while (iter < maxIter) {
                double zr2 = zr * zr - zi * zi + cr;
                double zi2 = 2 * zr * zi + ci;
                zr = zr2; zi = zi2;
                if (zr*zr + zi*zi > 4.0) break;
                ++iter;
            }
            float t = (iter >= maxIter) ? 0.0f : static_cast<float>(iter) / maxIter;
            uint8_t cr8, cg8, cb8;
            colorize(scheme, t, cr8, cg8, cb8);
            size_t o = (static_cast<size_t>(j) * w + i) * 4;
            data[o+0] = cb8; data[o+1] = cg8; data[o+2] = cr8; data[o+3] = 255;
        }
    }
    std::string name = "bbfx_mandelbrot_" + std::to_string(w) + "x" + std::to_string(h)
                       + "_" + std::to_string(maxIter);
    return writeRGBATexture(name, w, h, data);
}

std::string NoiseGenerator::juliaTexture(int w, int h,
                                                double cReal, double cImag,
                                                double zoom, int maxIter,
                                                const std::string& scheme) {
    if (w <= 0 || h <= 0 || zoom <= 0) return {};
    std::vector<uint8_t> data(static_cast<size_t>(w) * h * 4);
    double span = 3.0 / zoom;
    double minX = -span * 0.5;
    double minY = -span * 0.5 * h / w;
    double stepX = span / w;
    double stepY = span * h / w / h;
    for (int j = 0; j < h; ++j) {
        double zi0 = minY + j * stepY;
        for (int i = 0; i < w; ++i) {
            double zr0 = minX + i * stepX;
            double zr = zr0, zi = zi0; int iter = 0;
            while (iter < maxIter) {
                double zr2 = zr * zr - zi * zi + cReal;
                double zi2 = 2 * zr * zi + cImag;
                zr = zr2; zi = zi2;
                if (zr*zr + zi*zi > 4.0) break;
                ++iter;
            }
            float t = (iter >= maxIter) ? 0.0f : static_cast<float>(iter) / maxIter;
            uint8_t cr8, cg8, cb8;
            colorize(scheme, t, cr8, cg8, cb8);
            size_t o = (static_cast<size_t>(j) * w + i) * 4;
            data[o+0] = cb8; data[o+1] = cg8; data[o+2] = cr8; data[o+3] = 255;
        }
    }
    std::string name = "bbfx_julia_" + std::to_string(w) + "x" + std::to_string(h)
                       + "_" + std::to_string(maxIter);
    return writeRGBATexture(name, w, h, data);
}

} // namespace bbfx
