#pragma once
#include <string>
#include <functional>
#include <sol/forward.hpp>

namespace bbfx { class StudioEngine; }

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
    };
    FaderSlot mFaders[8];
    RecordValueCb mRecordValueCb;
    bool mIsRecording = false;
    float mCurrentBeat = 0.0f;

    float mBPM = 120.0f;
    float mRMS = 0.0f;
    float mBands[3] = {}; // low, mid, high

    bool mTriggerStates[16] = {};
    std::string mTriggerChords[16];
};

} // namespace bbfx
