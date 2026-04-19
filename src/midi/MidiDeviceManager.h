#pragma once
#include "MidiMessage.h"
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>

class RtMidiIn;
class RtMidiOut;

namespace bbfx {

/// Manages MIDI device enumeration, open/close, message polling, and hot-plug detection.
/// Thread-safe: rtmidi callbacks push to a queue, poll() drains from main thread.
class MidiDeviceManager {
public:
    static MidiDeviceManager* instance();

    MidiDeviceManager();
    ~MidiDeviceManager();

    // ── Device enumeration ──────────────────────────────────────────────
    int getInputDeviceCount() const;
    int getOutputDeviceCount() const;
    std::string getInputDeviceName(int index) const;
    std::string getOutputDeviceName(int index) const;
    std::vector<std::string> getInputDeviceNames() const;
    std::vector<std::string> getOutputDeviceNames() const;

    // ── Open / Close ────────────────────────────────────────────────────
    bool openInput(int index);
    void closeInput(int index);
    bool openOutput(int index);
    void closeOutput(int index);
    void openAll();
    void closeAll();
    bool isInputOpen(int index) const;

    // ── Message polling (main thread) ───────────────────────────────────
    /// Drains all pending messages from the receive queue. Call once per frame.
    std::vector<MidiMessage> poll();

    /// Inject a virtual message (for testing without hardware).
    void injectMessage(const MidiMessage& msg);

    // ── Send (output) ───────────────────────────────────────────────────
    void sendMessage(int outputDeviceId, uint8_t status, uint8_t data1, uint8_t data2);

    // ── Hot-plug ────────────────────────────────────────────────────────
    /// Call each frame to detect device additions/removals.
    void checkHotPlug();

    // ── State cache (v3.5 Lot M) ────────────────────────────────────────
    // Populated from the rtmidi callback thread (same mutex as queue) so
    // every CC/note change is visible from the main thread without
    // consuming the queue. `bbfx.midi.getCC` / `isNoteOn` read from this.
    // Channel is 1-16; any out-of-range value returns the default.

    /// Last CC value seen on (channel, cc), normalized 0..1. Returns 0 if
    /// the CC has never been observed on that channel.
    float getLastCCValue(int channel, int cc) const;

    /// True iff the last Note-On for (channel, note) has not yet been
    /// followed by a Note-Off (or Note-On with velocity 0).
    bool isNoteDown(int channel, int note) const;

    /// Velocity (0..1) of the most recent Note-On for (channel, note),
    /// or 0 if it was released.
    float getNoteVelocity(int channel, int note) const;

private:
    static void midiCallback(double timestamp, std::vector<unsigned char>* message, void* userData);

    struct CallbackData {
        MidiDeviceManager* manager;
        int deviceIndex;
    };

    struct InputDevice {
        int index;
        std::string name;
        std::unique_ptr<RtMidiIn> rtIn;
        std::unique_ptr<CallbackData> cbData; // persistent userData for rtmidi callback
        bool open = false;
    };

    struct OutputDevice {
        int index;
        std::string name;
        std::unique_ptr<RtMidiOut> rtOut;
        bool open = false;
    };

    std::vector<InputDevice> mInputDevices;
    std::vector<OutputDevice> mOutputDevices;

    // Thread-safe message queue (rtmidi callback → main thread poll)
    std::queue<MidiMessage> mMessageQueue;
    mutable std::mutex mQueueMutex;

    // v3.5 Lot M state cache — keyed by (channel-1)*128+ccOrNote.
    // Written under mQueueMutex from midiCallback / injectMessage,
    // read under the same mutex from accessors above.
    std::vector<float> mCCCache;           // size 16*128, values in 0..1
    std::vector<float> mNoteVelocity;      // size 16*128, 0 if note off
    std::vector<uint8_t> mNoteDown;        // size 16*128, 1 if held

    // Hot-plug detection
    int mLastInputCount = 0;
    int mHotPlugCounter = 0;

    // Singleton
    static MidiDeviceManager* sInstance;
};

} // namespace bbfx
