#pragma once
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <sol/forward.hpp>

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
    };
    std::vector<std::array<TriggerSlot, 16>> mTriggerPages;
    int mCurrentTriggerPage = 0;

    void executeTriggerAction(const std::string& action);

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
};

} // namespace bbfx
