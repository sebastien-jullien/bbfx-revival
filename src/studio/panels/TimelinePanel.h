#pragma once
#include <string>
#include <vector>

namespace bbfx { class StudioEngine; }

namespace bbfx {

struct ChordBlock {
    std::string name;
    float startBeat = 0.0f;
    float endBeat   = 4.0f;
    float hue       = 0.0f; // 0..1
};

/// Timeline panel: beat/bar markers, chord state blocks, transport, BPM, audio spectrum.
class TimelinePanel {
public:
    TimelinePanel() = default;

    void render(StudioEngine* engine);

    bool isPaused() const { return mPaused; }

private:
    void ensureDefaultChords();
    void renderTransport(StudioEngine* engine);
    void renderBPMDisplay();
    void renderMarkers(float pixelsPerBeat, float currentBeat);
    void renderChordBlocks(float pixelsPerBeat);
    void renderAudioSpectrum();

    float mBPM = 120.0f;
    float mScrollX = 0.0f;
    bool  mRecording = false;
    bool  mPaused    = false;

    std::vector<ChordBlock> mChordBlocks; // populated from Lua chord system
    float mSpectrumBands[8] = {};         // filled from AudioAnalyzer
    int mDraggedBlock = -1;
};

} // namespace bbfx
