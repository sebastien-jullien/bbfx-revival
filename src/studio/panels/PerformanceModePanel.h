#pragma once
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <sol/forward.hpp>
#include "../DagSnapshot.h"
#include "../ZoneSnapshot.h"

namespace Ogre { class Camera; }
namespace bbfx { class StudioEngine; class CompositorStackPanel; class SurfaceMap; class OutputManager; }

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

    /// Trigger the PANIC action from external code (e.g. StudioApp::panicAll). (v3.4 Lot M)
    void triggerPanic();

private:
    void renderTriggerGrid();
    void renderFaders();
    void renderVUMeters();
    void renderBPMOverlay();
    void renderPanicButton();

    sol::state& mLua;
    StudioEngine* mCurrentEngine = nullptr; ///< Transient, valid only during render()

    struct FaderSlot {
        std::string nodeName;
        std::string portName;
        float value = 0.0f;
        float minVal = 0.0f;
        float maxVal = 1.0f;
        // MIDI binding (v3.3)
        int midiCC = -1;           // CC number (-1 = no binding)
        int midiChannel = -1;      // channel (-1 = any)
        int midiDeviceId = -1;     // device (-1 = any)
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
        // MIDI binding (v3.3)
        int midiNote = -1;         // note number (-1 = no binding)
        int midiChannel = -1;      // channel (-1 = any)
        int midiDeviceId = -1;     // device (-1 = any)
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

public:
    // Auto-assign (v3.2.5)
    void autoAssignFaders(const std::vector<std::string>& createdNodeNames);
    /// Auto-assign all DAG nodes' heuristic ports to empty faders
    void autoAssignAllFaders();
private:

    // Preset wheel (v3.2.5)
    std::vector<std::string> mWheelPresets;
    int mWheelSelection = 0;
    void renderPresetWheel();
    std::vector<std::array<TriggerSlot, 16>> mTriggerPages;
    int mCurrentTriggerPage = 0;

    void executeTriggerAction(const std::string& action);
    void activateTrigger(TriggerSlot& trig);
    void deactivateTrigger(TriggerSlot& trig);

    // Rest snapshot for PANIC restore
    DagSnapshot mRestSnapshot;
    bool mRestCaptured = false;

    // Chord snapshots: chord name → DAG state
    std::map<std::string, DagSnapshot> mChordSnapshots;

    // Zone snapshots: chord name → zone warp/blend + camera state (v3.4 Lot O)
    std::map<std::string, ZoneSnapshot> mChordZoneSnapshots;
    ZoneSnapshot mRestZoneSnapshot;

    // Crossfader A/B (v3.2.5)
    DagSnapshot mSnapshotA, mSnapshotB;
    ZoneSnapshot mZoneSnapshotA, mZoneSnapshotB; ///< Zone snapshot crossfade (v3.4 Lot O)
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

    // Compositor chain (delegates to CompositorStackPanel)
    CompositorStackPanel* mCompositorStack = nullptr;
    bool mCompositorsPending = false;
    uint32_t mLastPerfW = 0, mLastPerfH = 0; // cached F5 viewport size for resize detection

public:
    void setCompositorStack(CompositorStackPanel* stack) { mCompositorStack = stack; }
    void applyCompositorChain(StudioEngine* engine);
    void removeCompositorChain(StudioEngine* engine);

public:

    /// Rebuild fader/trigger MIDI bindings from MidiLearnManager (single source of truth)
    void syncMidiBindingsFromManager();

    /// Force recapture of rest snapshot on next render (call after loadProject)
    void resetRestSnapshot() { mRestCaptured = false; }

    /// Chord snapshot management (for ChordSystem → DagSnapshot connection)
    void captureChordSnapshot(const std::string& chordName);
    void applyChordSnapshot(const std::string& chordName);
    void removeChordSnapshot(const std::string& chordName);
    bool hasChordSnapshot(const std::string& chordName) const { return mChordSnapshots.count(chordName) > 0; }
    auto& getChordSnapshots() { return mChordSnapshots; }
    const auto& getChordSnapshots() const { return mChordSnapshots; }
    void setChordSnapshots(const std::map<std::string, DagSnapshot>& snaps) { mChordSnapshots = snaps; }

    /// Zone snapshot management (v3.4 Lot O — Scene Switcher)
    void captureChordZoneSnapshot(const std::string& chordName, const SurfaceMap& map, const Ogre::Camera* cam);
    void applyChordZoneSnapshot(const std::string& chordName, SurfaceMap& map, OutputManager& mgr, Ogre::Camera* cam);
    void removeChordZoneSnapshot(const std::string& chordName);
    bool hasChordZoneSnapshot(const std::string& chordName) const { return mChordZoneSnapshots.count(chordName) > 0; }
    auto& getChordZoneSnapshots() { return mChordZoneSnapshots; }
    const auto& getChordZoneSnapshots() const { return mChordZoneSnapshots; }
    void setChordZoneSnapshots(const std::map<std::string, ZoneSnapshot>& snaps) { mChordZoneSnapshots = snaps; }
    ZoneSnapshot& getRestZoneSnapshot() { return mRestZoneSnapshot; }

    /// Returns the name of the active chord that has a zone snapshot, or empty string if none.
    std::string getActiveSceneChord() const {
        if (mTriggerPages.empty()) return {};
        for (auto& page : mTriggerPages) {
            for (auto& slot : page) {
                if (!slot.active) continue;
                if (slot.action.rfind("chord:", 0) == 0) {
                    std::string chordName = slot.action.substr(6);
                    if (mChordZoneSnapshots.count(chordName) > 0) return chordName;
                }
            }
        }
        return {};
    }

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
