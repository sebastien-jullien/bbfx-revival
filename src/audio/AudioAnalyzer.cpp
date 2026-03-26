#include "AudioAnalyzer.h"
#include "kiss_fft.h"
#include <cmath>
#include <algorithm>

namespace bbfx {

AudioAnalyzerNode::AudioAnalyzerNode(const string& name, AudioCaptureNode* captureNode)
    : AnimationNode(name), mCaptureNode(captureNode)
{
    // Create output ports
    addOutput(new AnimationPort("rms"));
    addOutput(new AnimationPort("peak"));
    for (int i = 0; i < NUM_BANDS; ++i) {
        addOutput(new AnimationPort("band_" + std::to_string(i)));
    }

    // Precompute Hann window
    int bufSize = captureNode->getSamples().size();
    mWindow.resize(bufSize);
    for (int i = 0; i < bufSize; ++i) {
        mWindow[i] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * i / (bufSize - 1)));
    }
}

void AudioAnalyzerNode::update() {
    if (!mCaptureNode->hasFreshData()) return;

    const auto& samples = mCaptureNode->getSamples();
    const int N = static_cast<int>(samples.size());
    if (N == 0) return;

    // Compute RMS and peak
    float sumSq = 0.0f;
    float maxAbs = 0.0f;
    for (int i = 0; i < N; ++i) {
        float s = samples[i];
        sumSq += s * s;
        float a = std::abs(s);
        if (a > maxAbs) maxAbs = a;
    }
    mRMS = std::sqrt(sumSq / N);
    mPeak = maxAbs;

    // Clamp to 0..1
    mRMS = std::min(mRMS, 1.0f);
    mPeak = std::min(mPeak, 1.0f);

    // Apply Hann window and compute FFT
    std::vector<float> windowed(N);
    for (int i = 0; i < N; ++i) {
        windowed[i] = samples[i] * mWindow[i];
    }
    auto spectrum = kiss_fft::magnitude_spectrum(windowed);

    // Divide spectrum into NUM_BANDS bands
    int specSize = static_cast<int>(spectrum.size());
    int bandSize = specSize / NUM_BANDS;
    if (bandSize < 1) bandSize = 1;

    for (int b = 0; b < NUM_BANDS; ++b) {
        int start = b * bandSize;
        int end = (b == NUM_BANDS - 1) ? specSize : (b + 1) * bandSize;
        float energy = 0.0f;
        for (int i = start; i < end; ++i) {
            energy += spectrum[i] * spectrum[i];
        }
        energy = std::sqrt(energy / (end - start));
        // Normalize: typical speech/music magnitude is ~0.01-0.1 after FFT normalization
        // Scale up by factor for 0..1 range
        mBands[b] = std::min(energy * 10.0f, 1.0f);
    }

    // Write to output ports
    mOutputs["rms"]->setValue(mRMS);
    mOutputs["peak"]->setValue(mPeak);
    for (int i = 0; i < NUM_BANDS; ++i) {
        mOutputs["band_" + std::to_string(i)]->setValue(mBands[i]);
    }

    fireUpdate();
}

} // namespace bbfx
