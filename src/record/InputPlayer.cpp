#include "InputPlayer.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>

namespace bbfx {

// Simple JSON value extractor (no external JSON lib dependency)
static std::string extractString(const std::string& line, const std::string& key) {
    std::string pattern = "\"" + key + "\":\"";
    auto pos = line.find(pattern);
    if (pos == std::string::npos) return "";
    pos += pattern.size();
    auto end = line.find('"', pos);
    return (end != std::string::npos) ? line.substr(pos, end - pos) : "";
}

static float extractFloat(const std::string& line, const std::string& key) {
    std::string pattern = "\"" + key + "\":";
    auto pos = line.find(pattern);
    if (pos == std::string::npos) return 0.0f;
    pos += pattern.size();
    try { return std::stof(line.substr(pos)); } catch (...) { return 0.0f; }
}

static int extractInt(const std::string& line, const std::string& key) {
    std::string pattern = "\"" + key + "\":";
    auto pos = line.find(pattern);
    if (pos == std::string::npos) return 0;
    pos += pattern.size();
    try { return std::stoi(line.substr(pos)); } catch (...) { return 0; }
}

void InputPlayer::play(const std::string& filename) {
    stop();
    mEvents.clear();
    mCurrentIndex = 0;
    mTime = 0.0f;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[InputPlayer] Cannot open: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        Event e;
        e.time = extractFloat(line, "t");
        e.type = extractString(line, "type");
        e.code = extractInt(line, "code");
        e.state = extractString(line, "state");
        e.value = extractFloat(line, "value");
        if (e.type == "axis") e.code = extractInt(line, "axis");
        mEvents.push_back(e);
    }

    mPlaying.store(true);
    std::cout << "[InputPlayer] Playing " << filename << " (" << mEvents.size() << " events)" << std::endl;
}

void InputPlayer::stop() {
    mPlaying.store(false);
    mCurrentIndex = 0;
    mTime = 0.0f;
}

std::vector<InputPlayer::Event> InputPlayer::getNextEvents(float dt) {
    std::vector<Event> result;
    if (!mPlaying.load()) return result;

    mTime += dt;

    while (mCurrentIndex < static_cast<int>(mEvents.size()) &&
           mEvents[mCurrentIndex].time <= mTime) {
        result.push_back(mEvents[mCurrentIndex]);
        mCurrentIndex++;
    }

    // End of replay
    if (mCurrentIndex >= static_cast<int>(mEvents.size())) {
        mPlaying.store(false);
        std::cout << "[InputPlayer] Replay finished (" << mEvents.size() << " events)" << std::endl;
    }

    return result;
}

} // namespace bbfx
