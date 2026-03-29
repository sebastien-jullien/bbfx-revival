#pragma once
#include "../commands/ChordCommands.h"
#include "../../record/InputRecorder.h"
#include <string>
#include <vector>
#include <memory>

namespace bbfx { class StudioEngine; }

namespace bbfx {

/// Timeline panel: beat/bar markers, chord state blocks, transport, BPM, audio spectrum.
class TimelinePanel {
public:
    TimelinePanel() = default;

    void render(StudioEngine* engine);

    bool isPaused() const { return mPaused; }
    void togglePause() { mPaused = !mPaused; }
    void stop();
    bool isRecording() const { return mRecording; }
    InputRecorder* getRecorder() { return mRecorder.get(); }

    const std::vector<bbfx::ChordBlock>& getChordBlocks() const { return mChordBlocks; }
    void setChordBlocks(const std::vector<bbfx::ChordBlock>& blocks) { mChordBlocks = blocks; }

private:
    void ensureDefaultChords();
    void renderTransport(StudioEngine* engine);
    void renderBPMDisplay();
    void renderMarkers(float pixelsPerBeat, float currentBeat);
    void renderChordBlocks(float pixelsPerBeat);
    void renderAudioSpectrum();

    float mBPM = 120.0f;
    float mPixelsPerBeat = 40.0f;
    float mScrollX = 0.0f;
    bool  mRecording = false;
    bool  mPaused    = false;
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
};

} // namespace bbfx
