#pragma once
#include <chrono>
#include <cstdint>

namespace bbfx {

/// MIDI Clock Generator — outputs 24 PPQN timing ticks, Start, Stop, and Continue messages.
///
/// Usage (main thread only — no thread safety needed):
///   gen.start(120.0f);  // Send 0xFA, begin ticking at 120 BPM
///   gen.update();       // Call each frame — sends pending 0xF8 ticks
///   gen.stop();         // Send 0xFC, halt ticking
///   gen.setBpm(130.0f); // Change BPM while running (seamless tempo change)
///
/// Sends messages to the first open MIDI output via MidiDeviceManager::sendMessage().
class MidiClockGenerator {
public:
    MidiClockGenerator() = default;
    ~MidiClockGenerator() { if (mRunning) stop(); }

    /// Start sending clock (sends 0xFA then begins 0xF8 ticks).
    void start(float bpm);
    /// Stop sending clock (sends 0xFC).
    void stop();
    /// Continue after stop (sends 0xFB).
    void resume();
    /// Change tempo without stopping.
    void setBpm(float bpm);

    /// Call once per frame from the main thread — sends all pending 0xF8 ticks.
    void update();

    bool isRunning() const { return mRunning; }
    float getBpm()   const { return mBpm; }

    /// MIDI system realtime message bytes
    static constexpr uint8_t MSG_START    = 0xFA;
    static constexpr uint8_t MSG_CONTINUE = 0xFB;
    static constexpr uint8_t MSG_STOP     = 0xFC;
    static constexpr uint8_t MSG_CLOCK    = 0xF8; ///< Timing Clock (24 PPQN)

private:
    void sendRealtime(uint8_t msg);
    void computeInterval();

    bool   mRunning  = false;
    bool   mStarted  = false;  ///< True after start() called (for CONTINUE protocol guard)
    float  mBpm      = 120.0f;
    double mIntervalMs = 0.0;  ///< ms between ticks = 60000 / (bpm * 24)

    using Clock = std::chrono::steady_clock;
    Clock::time_point mNextTick;
};

} // namespace bbfx
