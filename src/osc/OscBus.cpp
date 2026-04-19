#include "OscBus.h"

#include <algorithm>
#include <iostream>

#include "../midi/MidiMessage.h"
#include "../network/UdpServer.h"

namespace bbfx {

OscBus& OscBus::instance() {
    static OscBus sInstance;
    return sInstance;
}

OscBus::OscBus() = default;

OscBus::~OscBus() {
    shutdown();
}

void OscBus::shutdown() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mServer) {
        mServer->stop();
        mServer.reset();
    }
    mSubs.clear();
    mMidiLearnCbs.clear();
    mLastValues.clear();
}

bool OscBus::listen(int port) {
    if (port <= 0 || port > 65535) return false;
    std::lock_guard<std::mutex> lock(mMutex);
    if (mServer && mCurrentPort == port) return true;
    if (mServer) mServer->stop();
    mServer = std::make_unique<UdpServer>();
    if (!mServer->start(port)) {
        std::cerr << "[OscBus] listen(" << port << ") failed" << std::endl;
        mServer.reset();
        return false;
    }
    mCurrentPort = port;
    std::cout << "[OscBus] listening on port " << port << std::endl;
    return true;
}

void OscBus::tick() {
    std::vector<OscMessage> msgs;
    std::vector<Subscription> subsCopy;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mServer) return;
        msgs = mServer->poll();
        for (auto& m : msgs) {
            if (!m.args.empty()) {
                mLastValues[m.address] = m.args.front();
            }
        }
        subsCopy = mSubs;  // copy under lock, dispatch without
    }
    for (auto& m : msgs) {
        for (auto& s : subsCopy) {
            if (match(s.pattern, m.address)) {
                // Build a simple table of args for the callback.
                try {
                    if (!m.args.empty()) {
                        const auto& arg0 = m.args.front();
                        if (std::holds_alternative<float>(arg0))
                            s.cb(m.address, std::get<float>(arg0));
                        else if (std::holds_alternative<int32_t>(arg0))
                            s.cb(m.address, std::get<int32_t>(arg0));
                        else
                            s.cb(m.address, std::get<std::string>(arg0));
                    } else {
                        s.cb(m.address);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[OscBus] callback error on " << s.pattern
                              << ": " << e.what() << std::endl;
                }
            }
        }
    }
}

int OscBus::on(const std::string& pattern, sol::function cb) {
    std::lock_guard<std::mutex> lock(mMutex);
    int h = mNextHandle++;
    mSubs.push_back(Subscription{ h, pattern, std::move(cb) });
    return h;
}

void OscBus::off(int handle) {
    std::lock_guard<std::mutex> lock(mMutex);
    mSubs.erase(std::remove_if(mSubs.begin(), mSubs.end(),
        [handle](const Subscription& s) { return s.handle == handle; }),
        mSubs.end());
}

std::optional<OscArg> OscBus::last(const std::string& address) const {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mLastValues.find(address);
    if (it == mLastValues.end()) return std::nullopt;
    return it->second;
}

std::vector<std::string> OscBus::discoveredAddresses() const {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mServer) return {};
    return mServer->getDiscoveredAddresses();
}

bool OscBus::match(const std::string& pattern, const std::string& address) {
    // Very small glob: '*' matches one path segment, exact match otherwise.
    // Implementation — walk pattern and address in lockstep, splitting on '/'.
    auto split = [](const std::string& s) {
        std::vector<std::string> out;
        size_t pos = 0;
        while (pos <= s.size()) {
            auto next = s.find('/', pos);
            if (next == std::string::npos) {
                out.push_back(s.substr(pos));
                break;
            }
            out.push_back(s.substr(pos, next - pos));
            pos = next + 1;
        }
        return out;
    };
    auto p = split(pattern);
    auto a = split(address);
    if (p.size() != a.size()) return false;
    for (size_t i = 0; i < p.size(); ++i) {
        if (p[i] == "*") continue;
        if (p[i] != a[i]) return false;
    }
    return true;
}

// ── MIDI learn bridge ──────────────────────────────────────────────────────

void OscBus::registerMidiLearnCallback(const std::string& paramName, sol::function cb) {
    std::lock_guard<std::mutex> lock(mMutex);
    mMidiLearnCbs[paramName] = std::move(cb);
}

void OscBus::pumpMidiLearn(const MidiMessage* msgs, size_t n) {
    if (!msgs || n == 0) return;
    std::unordered_map<std::string, sol::function> cbsCopy;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        cbsCopy.swap(mMidiLearnCbs);  // one-shot — consume all pending cbs
    }
    if (cbsCopy.empty()) return;
    // Pick the first CC or Note message and invoke every pending cb once.
    for (size_t i = 0; i < n; ++i) {
        const auto& m = msgs[i];
        uint8_t t = m.type();
        if (t == MidiMessage::ControlChange || t == MidiMessage::NoteOn) {
            const char* kind = (t == MidiMessage::ControlChange) ? "cc" : "note";
            float value = m.data2 / 127.0f;
            for (auto& kv : cbsCopy) {
                try {
                    kv.second(kind, m.channel, (int)m.data1, value);
                } catch (const std::exception& e) {
                    std::cerr << "[OscBus] midi learn cb error: " << e.what() << std::endl;
                }
            }
            return;
        }
    }
    // Nothing matched — restore the pending callbacks for next frame.
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto& kv : cbsCopy) mMidiLearnCbs[kv.first] = kv.second;
}

} // namespace bbfx
