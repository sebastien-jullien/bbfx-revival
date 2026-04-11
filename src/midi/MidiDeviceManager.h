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

    // Hot-plug detection
    int mLastInputCount = 0;
    int mHotPlugCounter = 0;

    // Singleton
    static MidiDeviceManager* sInstance;
};

} // namespace bbfx
