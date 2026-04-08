#pragma once
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <sol/forward.hpp>
#include "../DagSnapshot.h"

namespace bbfx { class StudioEngine; class CompositorStackPanel; }

namespace bbfx {

/// Performance Mode (F5): fullscreen viewport, trigger grid, faders, VU meters, BPM overlay, panic.
class PerformanceModePanel {
public:
    explicit PerformanceModePanel(sol::state& lua);

    void render(StudioEngine* engine);

    /// Callback for recording fader values to automation
    using RecordValueCb = std::function<void(const std::string& nodeName,
        const std::string& portName, float value, float beat)>;
    void setRecordValueCallback(RecordValueCb cb) { mRecordValueCb = std::move(cb); }
    void setRecordingState(bool rec, float beat) { mIsRecording = rec; mCurrentBeat = beat; }

private:
    void renderTriggerGrid();
    void renderFaders();
    void renderVUMeters();
    void renderBPMOverlay();
    void renderPanicButton();

    sol::state& mLua;

    struct FaderSlot {
        std::string nodeName;
        std::string portName;
        float value = 0.0f;
        float minVal = 0.0f;
        float maxVal = 1.0f;
    };
    FaderSlot mFaders[8];
    RecordValueCb mRecordValueCb;
    bool mIsRecording = false;
    float mCurrentBeat = 0.0f;

    float mBPM = 120.0f;
    float mRMS = 0.0f;
    float mBands[3] = {}; // low, mid, high

    struct TriggerSlot {
        std::string label;
        std::string action;     // "chord:X", "enable:X", "disable:X", "compositor:X"
        bool momentary = false; // true = active on press, off on release
        float hue = 0.0f;      // HSV hue 0-1 for button color
        bool active = false;    // current state
        std::vector<std::string> macroActions; // v3.2.5: multi-action sequence (empty = single action)
    };

    // MacroRunner state machine for deferred execution of macro sequences
    struct MacroRunner {
        std::vector<std::string> pendingActions;
        int currentIndex = 0;
        float targetBeat = 0.0f;
        bool running = false;
        void start(const std::vector<std::string>& actions) {
            pendingActions = actions;
            currentIndex = 0;
            running = true;
            targetBeat = 0.0f;
        }
        void cancel() { running = false; pendingActions.clear(); currentIndex = 0; }
    };
    MacroRunner mMacroRunner;

    // Auto-assign (v3.2.5)
    void autoAssignFaders(const std::vector<std::string>& createdNodeNames);

    // Preset wheel (v3.2.5)
    std::vector<std::string> mWheelPresets;
    int mWheelSelection = 0;
    void renderPresetWheel();
    std::vector<std::array<TriggerSlot, 16>> mTriggerPages;
    int mCurrentTriggerPage = 0;

    void executeTriggerAction(const std::string& action);

    // Crossfader A/B (v3.2.5)
    DagSnapshot mSnapshotA, mSnapshotB;
    float mCrossfadePos = 0.0f;
    bool mCrossfadeActive = false;
    std::string mSnapshotAName = "(empty)";
    std::string mSnapshotBName = "(empty)";
    // Auto-crossfade
    bool mAutoFading = false;
    float mAutoStartBeat = 0.0f;
    float mAutoDurationBeats = 4.0f;
    float mAutoStartPos = 0.0f;
    float mAutoTargetPos = 1.0f;
    bool mAutoBounce = false;

    void renderCrossfader();

    // Fader learn mode (v3.2.4)
    int mLearnFaderIndex = -1;

    // Compositor chain (delegates to CompositorStackPanel)
    CompositorStackPanel* mCompositorStack = nullptr;
    bool mCompositorsPending = false;
    uint32_t mLastPerfW = 0, mLastPerfH = 0; // cached F5 viewport size for resize detection

public:
    void setCompositorStack(CompositorStackPanel* stack) { mCompositorStack = stack; }
    void applyCompositorChain(StudioEngine* engine);
    void removeCompositorChain(StudioEngine* engine);

public:
    // For fader learn callback from InspectorPanel
    void onLearnParam(const std::string& nodeName, const std::string& portName);

    // Accessors for save/load
    auto& getTriggerPages() { return mTriggerPages; }
    const auto& getTriggerPages() const { return mTriggerPages; }
    int getCurrentTriggerPage() const { return mCurrentTriggerPage; }
    auto& getFaders() { return mFaders; }
    const auto& getFaders() const { return mFaders; }
    auto& getWheelPresets() { return mWheelPresets; }
    const auto& getWheelPresets() const { return mWheelPresets; }

    // Test automation accessors (v3.2.5)
    void setCrossfadePos(float pos) { mCrossfadePos = pos; mCrossfadeActive = true; }
    void startAutoCrossfade(float beats) {
        mAutoFading = true; mAutoDurationBeats = beats;
        mAutoStartBeat = mCurrentBeat; mAutoStartPos = mCrossfadePos;
        mAutoTargetPos = (mCrossfadePos < 0.5f) ? 1.0f : 0.0f;
        mCrossfadeActive = true;
    }
    void executeTriggerActionPublic(const std::string& action) { executeTriggerAction(action); }
};

} // namespace bbfx
