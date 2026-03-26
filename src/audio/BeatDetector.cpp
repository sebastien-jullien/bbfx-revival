#include "BeatDetector.h"
#include <algorithm>
#include <numeric>

namespace bbfx {

BeatDetectorNode::BeatDetectorNode(const string& name, AudioAnalyzerNode* analyzer)
    : AnimationNode(name), mAnalyzer(analyzer)
{
    addInput(new AnimationPort("sensitivity"));
    addInput(new AnimationPort("dt"));
    addOutput(new AnimationPort("beat"));
    addOutput(new AnimationPort("bpm"));

    // Default sensitivity
    mInputs["sensitivity"]->setValue(0.5f);
}

void BeatDetectorNode::update() {
    // Advance time
    float dt = 0.0f;
    if (mInputs["dt"]) {
        dt = mInputs["dt"]->getValue();
    }
    mTime += dt;

    float sensitivity = mInputs["sensitivity"]->getValue();
    float energy = mAnalyzer->getRMS();

    // Add to history
    mEnergyHistory.push_back(energy);
    if (static_cast<int>(mEnergyHistory.size()) > HISTORY_SIZE) {
        mEnergyHistory.pop_front();
    }

    // Compute moving average
    float avg = 0.0f;
    if (!mEnergyHistory.empty()) {
        for (float e : mEnergyHistory) avg += e;
        avg /= static_cast<float>(mEnergyHistory.size());
    }

    // Onset detection: current energy > average * threshold
    float threshold = 1.0f + sensitivity * 2.0f; // sensitivity 0..1 → threshold 1..3
    bool isBeat = (energy > avg * threshold) && (energy > 0.01f);

    // Anti-bounce: minimum interval between beats
    if (isBeat && (mTime - mLastBeatTime) < MIN_BEAT_INTERVAL) {
        isBeat = false;
    }

    if (isBeat) {
        // Record beat time for BPM estimation
        if (mLastBeatTime > 0.0f) {
            mBeatTimes.push_back(mTime - mLastBeatTime);
            // Keep last 16 intervals
            while (mBeatTimes.size() > 16) {
                mBeatTimes.pop_front();
            }
            // Estimate BPM from average interval
            if (!mBeatTimes.empty()) {
                float avgInterval = 0.0f;
                for (float t : mBeatTimes) avgInterval += t;
                avgInterval /= static_cast<float>(mBeatTimes.size());
                if (avgInterval > 0.0f) {
                    mCurrentBPM = 60.0f / avgInterval;
                    // Clamp to reasonable range
                    mCurrentBPM = std::clamp(mCurrentBPM, 40.0f, 300.0f);
                }
            }
        }
        mLastBeatTime = mTime;
    }

    // Write outputs
    mOutputs["beat"]->setValue(isBeat ? 1.0f : 0.0f);
    mOutputs["bpm"]->setValue(mCurrentBPM);

    if (isBeat) {
        fireUpdate();
    }
}

} // namespace bbfx
