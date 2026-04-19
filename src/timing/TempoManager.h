#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace bbfx {

/// v3.5 Lot N — single source of truth for beat-synced tempo.
///
/// Unifies three potential BPM sources (AUDIO via BeatDetectorNode,
/// MIDI_CLOCK via MidiClockGenerator, or MANUAL set by the user) behind
/// one stable API that plugins and nodes can consume via `bbfx.tempo.*`.
///
/// Callbacks are scheduled on "next beat", "next bar" (4 beats), or
/// "every Nth subdivision" and fire during update() (main-thread only).
class TempoManager {
public:
    enum class Source { AUDIO, MIDI_CLOCK, MANUAL };
    static const char* sourceName(Source s);
    static Source      sourceFromString(const std::string& s);

    static TempoManager& instance();

    void  setSource(Source s);
    Source getSource() const;

    // Manual BPM only used when source == MANUAL.
    void  setManualBPM(float bpm);
    float getManualBPM() const;

    // Effective BPM (picks the right source).
    float getBPM() const;

    // Pulled from the engine's RootTimeNode — accumulating beats since
    // project start. Wraps to 0..1 for `getBeatPhase()`, stays unbounded
    // for `getBeat()` (useful for bar math : `math.floor(beat/4)`).
    float getBeat()       const;
    float getBeatPhase()  const;

    /// Call once per frame from the engine update loop — computes the
    /// beat increment and fires any due callbacks.
    void update();

    // Callback registration — returns an integer handle.
    using Callback = std::function<void()>;
    int onNextBeat(Callback cb);
    int onNextBar (Callback cb);
    int onSubdivision(int n, Callback cb);
    void off(int handle);

private:
    TempoManager() = default;
    TempoManager(const TempoManager&) = delete;
    TempoManager& operator=(const TempoManager&) = delete;

    struct Pending {
        int     handle;
        float   fireAtBeat;   // < 0 means always-fire (subdivision)
        int     subdivision;   // 0 = one-shot
        Callback cb;
    };

    mutable std::mutex mMutex;
    Source mSource     = Source::MANUAL;
    float  mManualBPM  = 120.0f;
    float  mLastBeat   = 0.0f;
    int    mNextHandle = 1;
    std::vector<Pending> mPending;
};

} // namespace bbfx
