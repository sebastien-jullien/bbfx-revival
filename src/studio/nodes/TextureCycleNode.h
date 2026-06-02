#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <random>
#include <string>
#include <vector>

namespace bbfx {

/// TextureCycleNode — ordered list of texture names with current index, transition
/// to next, and selectable advance modes (sequential / random / bpm_synced /
/// audio_triggered). Reproduction of the BBFx 2006 `TextureCycle:next()` Lua
/// pattern (cf. textureset.lua:120-134).
///
/// Outputs:
///   - current_texture (STRING port — encoded as numeric index in DAG, real value via ParamSpec mirror)
///   - next_texture
///   - transition_progress (0..1)
///   - current_index (INT-as-FLOAT)
///
/// Triggers (input ports, edge-detected on > 0.5):
///   - next, prev, goto_index, beat (audio_triggered mode)
class TextureCycleNode : public AnimationNode {
public:
    enum class Mode { Sequential, Random, BpmSynced, AudioTriggered };

    explicit TextureCycleNode(const std::string& name);
    ~TextureCycleNode() override = default;

    void update() override;
    std::string getTypeName() const override { return "TextureCycleNode"; }

    // Lua/UI accessors used by tests and the Inspector.
    void setTextures(const std::vector<std::string>& list);
    const std::vector<std::string>& getTextures() const { return mTextures; }
    int  getCurrentIndex() const { return mCurrentIndex; }
    int  getNextIndex()    const { return mNextIndex; }
    float getTransitionProgress() const { return mTransitionProgress; }
    const std::string& getCurrentTexture() const;
    const std::string& getNextTexture()    const;

    // Public trigger helpers (used by the test suite to bypass DAG plumbing).
    void triggerNext();
    void triggerPrev();
    void triggerGoto(int idx);

private:
    ParamSpec mSpec;
    std::vector<std::string> mTextures;
    int  mCurrentIndex = 0;
    int  mNextIndex = 0;
    float mTransitionProgress = 0.0f;
    float mTransitionTime = 1.0f;
    Mode mMode = Mode::Sequential;
    float mAutoAdvanceBpm = 0.0f;
    int  mRandomSeed = 0;
    std::mt19937 mRng;
    bool mRngInitialized = false;

    // Edge detection state
    bool mPrevNextState = false;
    bool mPrevPrevState = false;
    int  mPrevGotoIndex = -1;
    int  mPrevIndexInput = -1;  // last value seen on the 'index' input port
    bool mPrevBeatState = false;

    // BPM accumulator
    float mBpmAccumulator = 0.0f;
    float mPrevDt = 0.0f;

    int  pickRandomNext();
    void seedRngIfNeeded();
    void commitPendingTransition();   // snap to mNextIndex before queueing a new fade
};

} // namespace bbfx
