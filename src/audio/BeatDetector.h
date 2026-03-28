#pragma once

#include "../core/AnimationNode.h"
#include "AudioAnalyzer.h"
#include <deque>

namespace bbfx {

/// Detects musical beats via onset detection and estimates BPM.
/// Uses energy threshold against a moving average.
/// Outputs: beat (1.0 on onset frame, 0.0 otherwise), bpm (estimated BPM).
class BeatDetectorNode : public AnimationNode {
public:
    BeatDetectorNode(const string& name, AudioAnalyzerNode* analyzer);
    ~BeatDetectorNode() override = default;

    void update() override;
    std::string getTypeName() const override { return "BeatDetectorNode"; }

private:
    AudioAnalyzerNode* mAnalyzer;

    // Energy history for moving average
    std::deque<float> mEnergyHistory;
    static constexpr int HISTORY_SIZE = 43; // ~0.7s at 60fps

    // Beat timing for BPM estimation
    std::deque<float> mBeatTimes; // timestamps of recent beats (seconds)
    float mTime = 0.0f;
    float mLastBeatTime = -1.0f;
    static constexpr float MIN_BEAT_INTERVAL = 0.2f; // 300 BPM max

    float mCurrentBPM = 0.0f;
};

} // namespace bbfx
