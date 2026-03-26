/*
 * Minimal FFT implementation for BBFx audio analysis.
 * Radix-2 Cooley-Tukey DIT FFT — public domain.
 * Supports power-of-2 sizes only (1024, 2048).
 */
#pragma once

#include <complex>
#include <vector>
#include <cmath>

namespace kiss_fft {

using Complex = std::complex<float>;

/// In-place radix-2 Cooley-Tukey FFT.
/// data must have power-of-2 size.
inline void fft(std::vector<Complex>& data) {
    const size_t N = data.size();
    if (N <= 1) return;

    // Bit-reversal permutation
    for (size_t i = 1, j = 0; i < N; ++i) {
        size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }

    // Cooley-Tukey butterfly
    for (size_t len = 2; len <= N; len <<= 1) {
        float ang = -2.0f * 3.14159265358979f / static_cast<float>(len);
        Complex wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < N; i += len) {
            Complex w(1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; ++j) {
                Complex u = data[i + j];
                Complex v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

/// Compute magnitude spectrum from real-valued samples.
/// Returns N/2 magnitudes (positive frequencies only).
inline std::vector<float> magnitude_spectrum(const std::vector<float>& samples) {
    const size_t N = samples.size();
    std::vector<Complex> data(N);
    for (size_t i = 0; i < N; ++i) {
        data[i] = Complex(samples[i], 0.0f);
    }
    fft(data);
    std::vector<float> mag(N / 2);
    for (size_t i = 0; i < N / 2; ++i) {
        mag[i] = std::abs(data[i]) / static_cast<float>(N);
    }
    return mag;
}

} // namespace kiss_fft
