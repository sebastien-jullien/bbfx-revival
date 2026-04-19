#include "TempoManager.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "../core/PrimitiveNodes.h"

namespace bbfx {

const char* TempoManager::sourceName(Source s) {
    switch (s) {
        case Source::AUDIO:       return "audio";
        case Source::MIDI_CLOCK:  return "midi_clock";
        case Source::MANUAL:      return "manual";
    }
    return "manual";
}

TempoManager::Source TempoManager::sourceFromString(const std::string& s) {
    if (s == "audio")       return Source::AUDIO;
    if (s == "midi_clock")  return Source::MIDI_CLOCK;
    return Source::MANUAL;
}

TempoManager& TempoManager::instance() {
    static TempoManager sInst;
    return sInst;
}

void TempoManager::setSource(Source s) {
    std::lock_guard<std::mutex> lock(mMutex);
    mSource = s;
}

TempoManager::Source TempoManager::getSource() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mSource;
}

void TempoManager::setManualBPM(float bpm) {
    if (bpm <= 0.0f) return;
    std::lock_guard<std::mutex> lock(mMutex);
    mManualBPM = bpm;
    // If we are on manual source, also propagate to RootTimeNode so that
    // the DAG-facing beat/beatfrac ports stay consistent with what Lua
    // sees.
    if (mSource == Source::MANUAL) {
        if (auto* rt = RootTimeNode::instance()) rt->setBPM(bpm);
    }
}

float TempoManager::getManualBPM() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mManualBPM;
}

float TempoManager::getBPM() const {
    std::lock_guard<std::mutex> lock(mMutex);
    switch (mSource) {
        case Source::MANUAL:
            return mManualBPM;
        case Source::MIDI_CLOCK: {
            // MidiClockGenerator is an outgoing clock (BBFx sends tick
            // messages to external gear). Incoming MIDI-clock sync is a
            // Lot Q feature: BBFx will average successive 0xF8 messages
            // from MidiDeviceManager::poll(). For now, a graph that
            // selects MIDI_CLOCK without that pipe falls back to the
            // DAG-level RootTimeNode BPM so existing clocks still work.
            if (auto* rt = RootTimeNode::instance()) {
                float v = rt->getBPM();
                if (v > 0.0f) return v;
            }
            return mManualBPM;
        }
        case Source::AUDIO: {
            // BeatDetector currently feeds RootTimeNode.setBPM from the
            // graph pipeline; reuse that as the source of truth so the
            // value stays coherent with the DAG-level beat port.
            if (auto* rt = RootTimeNode::instance()) {
                float v = rt->getBPM();
                if (v > 0.0f) return v;
            }
            return mManualBPM;
        }
    }
    return mManualBPM;
}

float TempoManager::getBeat() const {
    auto* rt = RootTimeNode::instance();
    if (!rt) return 0.0f;
    float bpm = getBPM();
    if (bpm <= 0.0f) return 0.0f;
    float total = static_cast<float>(rt->getTotalTime());
    return total * bpm / 60.0f;
}

float TempoManager::getBeatPhase() const {
    float b = getBeat();
    return b - std::floor(b);
}

void TempoManager::update() {
    float beat = getBeat();
    std::vector<Callback> toFire;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        float prev = mLastBeat;
        mLastBeat = beat;
        if (beat < prev) {
            // Reset detected (seek backwards or reset) — clear pending
            // one-shots but keep periodic subdivisions.
            mPending.erase(std::remove_if(mPending.begin(), mPending.end(),
                [](const Pending& p) { return p.subdivision == 0; }),
                mPending.end());
            return;
        }
        // Fire one-shots whose `fireAtBeat` is in (prev, beat].
        auto it = mPending.begin();
        while (it != mPending.end()) {
            bool fire = false;
            if (it->subdivision == 0) {
                if (it->fireAtBeat > prev && it->fireAtBeat <= beat) {
                    fire = true;
                }
            } else {
                // Periodic: fire each time an integer multiple of
                // (1 / subdivision) is crossed.
                float step = 1.0f / it->subdivision;
                float pStep = std::floor(prev / step);
                float cStep = std::floor(beat / step);
                if (cStep > pStep) fire = true;
            }
            if (fire) {
                toFire.push_back(it->cb);
                if (it->subdivision == 0) {
                    it = mPending.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }
    // Dispatch outside the lock so callbacks can register new handles.
    for (auto& cb : toFire) {
        try { cb(); }
        catch (const std::exception& e) {
            std::cerr << "[TempoManager] callback error: " << e.what() << std::endl;
        }
    }
}

int TempoManager::onNextBeat(Callback cb) {
    std::lock_guard<std::mutex> lock(mMutex);
    int h = mNextHandle++;
    float target = std::floor(mLastBeat) + 1.0f;
    mPending.push_back({ h, target, 0, std::move(cb) });
    return h;
}

int TempoManager::onNextBar(Callback cb) {
    std::lock_guard<std::mutex> lock(mMutex);
    int h = mNextHandle++;
    // Next multiple of 4 strictly greater than current beat.
    float target = std::floor(mLastBeat / 4.0f + 1.0f) * 4.0f;
    mPending.push_back({ h, target, 0, std::move(cb) });
    return h;
}

int TempoManager::onSubdivision(int n, Callback cb) {
    if (n < 1) n = 1;
    std::lock_guard<std::mutex> lock(mMutex);
    int h = mNextHandle++;
    mPending.push_back({ h, -1.0f, n, std::move(cb) });
    return h;
}

void TempoManager::off(int handle) {
    std::lock_guard<std::mutex> lock(mMutex);
    mPending.erase(std::remove_if(mPending.begin(), mPending.end(),
        [handle](const Pending& p) { return p.handle == handle; }),
        mPending.end());
}

} // namespace bbfx
