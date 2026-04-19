#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bbfx {

/// v3.5 Lot N — deterministic noise generator used by `bbfx.noise.*`.
///
/// All functions are pure and depend only on (x, y, z, w, seed). The
/// simplex family is the canonical Ken Perlin 2D/3D/4D implementation;
/// cellular noise returns the F1 distance (Worley); curl noise derives
/// a divergence-free vector from simplex gradients. Fractional Brownian
/// motion sums octaves of simplex.
///
/// All methods are `static` — no shared state. Callers supply their
/// own seed to control determinism.
class NoiseGenerator {
public:
    struct FbmOptions {
        int   octaves   = 4;
        float lacunarity = 2.0f;
        float gain       = 0.5f;
        int   seed       = 0;
    };

    // --- Simplex (Perlin) ------------------------------------------------
    // Return value range is approximately [-1, 1].
    static float simplex2D(float x, float y, int seed = 0);
    static float simplex3D(float x, float y, float z, int seed = 0);
    static float simplex4D(float x, float y, float z, float w, int seed = 0);

    // --- Cellular (Worley F1) -------------------------------------------
    // Returns the distance to the closest feature point, normalized so
    // that the expected range is roughly [0, 1].
    static float worley2D(float x, float y, int seed = 0);
    static float worley3D(float x, float y, float z, int seed = 0);

    // --- Curl (divergence-free) -----------------------------------------
    // Writes (outX, outY [, outZ]) with a divergence-free vector.
    static void  curl2D(float x, float y, int seed, float& outX, float& outY);
    static void  curl3D(float x, float y, float z, int seed,
                         float& outX, float& outY, float& outZ);

    // --- Fractional Brownian motion -------------------------------------
    static float fbm2D(float x, float y, const FbmOptions& opts);

    // --- Texture generation -----------------------------------------------
    // Fill `out` with `w*h` floats in [-1..1] sampled from simplex2D.
    // `scale` multiplies the world-space coord; seed is passed through.
    static void fillTexture2D(std::vector<float>& out,
                                 int w, int h,
                                 float scale, int seed);

    /// v3.5 Lot N I-1426 — create an OGRE texture filled with noise and
    /// return its name. Backed by fillTexture2D; maps to R8G8B8 so the
    /// texture can be sampled by any OGRE material unchanged.
    ///
    /// `kind` is "simplex", "worley" or "fbm" (case-insensitive).
    /// Opts order: scale, octaves (fbm), seed.
    /// Returns the texture name (empty on failure).
    static std::string generateTexture(int w, int h, const std::string& kind,
                                          float scale, int octaves, int seed);

    // --- Lot R Fractal generators (CPU-side, PF_A8R8G8B8 texture) ---
    /// Render a Mandelbrot set texture. `colorScheme` ∈
    /// {"rainbow","fire","ocean","grayscale"}. Returns the texture name.
    static std::string mandelbrotTexture(int w, int h,
                                             double centerX, double centerY,
                                             double zoom, int maxIter,
                                             const std::string& colorScheme);

    /// Render a Julia set texture at constant c = (cReal, cImag).
    static std::string juliaTexture(int w, int h,
                                        double cReal, double cImag,
                                        double zoom, int maxIter,
                                        const std::string& colorScheme);
};

} // namespace bbfx
