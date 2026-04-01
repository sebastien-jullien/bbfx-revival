#pragma once

#include <string>
#include <fstream>
#include <atomic>

namespace bbfx {

/// Records input events (key, axis, beat) with timestamps to a .bbfx-session file.
/// Format: JSON Lines (one JSON object per line), flushed after each event.
class InputRecorder {
public:
    InputRecorder() = default;
    ~InputRecorder();

    void start(const std::string& filename);
    void stop();
    bool isRecording() const { return mRecording.load(); }

    void advanceTime(float dt) { mTime += dt; }

    void recordKey(int code, const std::string& state);   // "press"/"release"
    void recordAxis(int axis, float value);
    void recordBeat();

    // v3.2 Studio events
    void recordChordActivate(const std::string& name);
    void recordChordDeactivate(const std::string& name);
    void recordFaderChange(int index, const std::string& node, const std::string& port, float value);
    void recordBPMChange(float bpm);
    void recordPresetInstantiate(const std::string& presetName);

private:
    std::ofstream mFile;
    std::atomic<bool> mRecording{false};
    float mTime = 0.0f;
};

} // namespace bbfx
