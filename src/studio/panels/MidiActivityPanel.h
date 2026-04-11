#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <deque>

namespace bbfx {

/// Real-time MIDI message monitor panel.
/// Shows incoming messages with color coding, filtering, and statistics.
class MidiActivityPanel {
public:
    void render();

private:
    struct LogEntry {
        double timestamp;
        int deviceId;
        int channel;
        uint8_t type;
        uint8_t data1, data2;
        std::string decoded; // human-readable
    };

    std::deque<LogEntry> mLog;
    static constexpr size_t kMaxEntries = 200;
    int mFilterDevice = -1;  // -1 = all
    int mFilterChannel = 0;  // 0 = all
    bool mAutoScroll = true;
    int mMessageCount = 0;
    float mMessagesPerSecond = 0.0f;
    float mMpsTimer = 0.0f;
    int mMpsCounter = 0;

    static std::string noteName(int noteNumber);
    static std::string typeName(uint8_t type);
};

} // namespace bbfx
