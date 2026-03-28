#pragma once

#include "../core/AnimationNode.h"
#include "AudioCapture.h"
#include <vector>
#include <cstdint>

namespace bbfx {

/// Analyzes audio samples from AudioCaptureNode using FFT.
/// Outputs: rms, peak, band_0..band_7 (8 frequency bands normalized 0..1).
class AudioAnalyzerNode : public AnimationNode {
public:
    AudioAnalyzerNode(const string& name, AudioCaptureNode* captureNode);
    ~AudioAnalyzerNode() override = default;

    void update() override;
    std::string getTypeName() const override { return "AudioAnalyzerNode"; }

    static constexpr int NUM_BANDS = 8;

    float getRMS() const { return mRMS; }
    float getPeak() const { return mPeak; }
    float getBand(int i) const { return (i >= 0 && i < NUM_BANDS) ? mBands[i] : 0.0f; }

private:
    AudioCaptureNode* mCaptureNode;
    std::vector<float> mWindow; // Hann window coefficients
    float mRMS = 0.0f;
    float mPeak = 0.0f;
    float mBands[NUM_BANDS] = {};
    float mPeakRMSSinceLog = 0.0f;
    uint64_t mLastLogTick = 0;
};

} // namespace bbfx
