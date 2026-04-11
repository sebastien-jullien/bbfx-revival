#pragma once
#include "../commands/ChordCommands.h"
#include "../../record/InputRecorder.h"
#include "../../core/AutomationData.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <memory>

namespace bbfx { class StudioEngine; }

namespace bbfx {

/// Timeline panel: beat/bar markers, chord blocks, automation lanes, cue markers, loop region.
class TimelinePanel {
public:
    TimelinePanel() = default;

    void render(StudioEngine* engine);

    bool isPaused() const { return mPaused; }
    void togglePause();
    void stop();
    bool isRecording() const { return mRecording; }
    InputRecorder* getRecorder() { return mRecorder.get(); }

    const std::vector<bbfx::ChordBlock>& getChordBlocks() const { return mChordBlocks; }
    void setChordBlocks(const std::vector<bbfx::ChordBlock>& blocks) { mChordBlocks = blocks; }

    AutomationData& getAutomation() { return mAutomation; }
    const AutomationData& getAutomation() const { return mAutomation; }

    /// Add a lane pre-assigned to a port (called from InspectorPanel "Add to Timeline")
    void addLaneForPort(const std::string& nodeName, const std::string& portName,
                        float minVal, float maxVal);

    /// Record a value change for a port (called from faders/Inspector during REC)
    void recordValue(const std::string& nodeName, const std::string& portName,
                     float value, float currentBeat);

    float getBPM() const { return mBPM; }
    void  changeBPM(float delta);

private:
    void ensureDefaultChords();
    void renderTransport(StudioEngine* engine);
    void renderBPMDisplay();
    void renderMarkers(float pixelsPerBeat, float currentBeat);
    void renderChordBlocks(float pixelsPerBeat);
    void renderCueMarkers(float pixelsPerBeat, float canvasTop, float canvasHeight);
    void renderLoopRegion(float pixelsPerBeat, float canvasTop);
    void renderAutomationLanes(float pixelsPerBeat, float currentBeat);
    void renderAudioSpectrum();
    void handleAutomationKeys(float currentBeat);

    float mBPM = 120.0f;
    float mPixelsPerBeat = 40.0f;
    float mScrollX = 0.0f;
    bool  mRecording = false;
    bool  mPaused    = false;
    bool  mRecordReplace = false;  // false = Overdub, true = Replace
    std::unique_ptr<InputRecorder> mRecorder;

    std::vector<bbfx::ChordBlock> mChordBlocks;
    float mSpectrumBands[8] = {};
    int mDraggedBlock = -1;

    // Chord editing state
    int mResizingBlock = -1;
    bool mResizingLeft = false;
    float mResizeOldStart = 0.0f, mResizeOldEnd = 0.0f;
    char mRenameChordBuf[128] = {};
    int mContextChordIdx = -1;

    // Automation data
    AutomationData mAutomation;

    // Automation UI state
    float mLaneScrollY = 0.0f;
    float mLaneZoomY = 1.0f;
    int mSelectedLane = -1;
    std::set<std::pair<int,int>> mSelectedKeyframes;
    int mSelectedCue = -1;

    // Keyframe drag state
    bool mDraggingKeyframe = false;
    int mDragKfLane = -1, mDragKfIdx = -1;
    float mDragKfOldBeat = 0, mDragKfOldValue = 0;

    // Box select state
    bool mBoxSelecting = false;
    ImVec2 mBoxSelectStart = {0, 0};

    // Tangent drag state
    bool mDraggingTangent = false;
    bool mDragTangentIsOut = true;  // true = out handle, false = in handle
    int mDragTangentLane = -1, mDragTangentKf = -1;
    float mDragTangentOldInB = 0, mDragTangentOldInV = 0;
    float mDragTangentOldOutB = 0, mDragTangentOldOutV = 0;

    // Loop drag state
    bool mDraggingLoopStart = false, mDraggingLoopEnd = false, mCreatingLoop = false;
    float mLoopDragOldStart = 0, mLoopDragOldEnd = 0;

    // Recording state
    std::map<std::string, float> mLastRecordedValues;  // portFullName → last recorded value

    // Cue counter
    int mCueCounter = 1;

    // Trigger event state
    float mPrevBeat = 0.0f;

    // Edit keyframe popup state
    bool mShowEditKfPopup = false;
    int mEditKfLane = -1, mEditKfIdx = -1;
    float mEditKfBeat = 0, mEditKfValue = 0;
    int mEditKfMode = 0;

    // LFO popup state
    bool mShowLFOPopup = false;
    int mLFOLane = -1;
    int mLFOWaveform = 0;   // 0=sine, 1=square, 2=triangle, 3=sawtooth
    float mLFOFreq = 4.0f;  // in beats
    float mLFOAmplitude = 1.0f;
    float mLFOOffset = 0.0f;
    float mLFOStartBeat = 0.0f;
    int mLFOCycles = 4;
};

} // namespace bbfx
