#pragma once

#include <string>
#include <vector>
#include <atomic>

namespace bbfx {

/// Replays a .bbfx-session file, providing events at the correct timestamps.
class InputPlayer {
public:
    struct Event {
        float time;
        std::string type;   // "key", "axis", "beat"
        int code = 0;
        std::string state;  // "press"/"release" for keys
        float value = 0.0f; // for axes
    };

    InputPlayer() = default;

    void play(const std::string& filename);
    void stop();
    bool isPlaying() const { return mPlaying.load(); }

    /// Advance time by dt, return events whose timestamp has been reached.
    std::vector<Event> getNextEvents(float dt);

private:
    std::vector<Event> mEvents;
    int mCurrentIndex = 0;
    float mTime = 0.0f;
    std::atomic<bool> mPlaying{false};
};

} // namespace bbfx
