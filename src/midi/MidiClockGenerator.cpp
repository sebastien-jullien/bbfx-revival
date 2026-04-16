#include "MidiClockGenerator.h"
#include "MidiDeviceManager.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace bbfx {

void MidiClockGenerator::computeInterval() {
    // 24 PPQN: interval = 60000 / (bpm * 24) milliseconds
    mIntervalMs = 60000.0 / (static_cast<double>(mBpm) * 24.0);
}

void MidiClockGenerator::sendRealtime(uint8_t msg) {
    auto* mgr = MidiDeviceManager::instance();
    if (!mgr) return;
    // Send to output device 0 (first open output); data bytes unused for realtime.
    mgr->sendMessage(0, msg, 0, 0);
}

void MidiClockGenerator::start(float bpm) {
    mBpm     = std::clamp(bpm > 0.f ? bpm : 120.f, 1.f, 999.f);
    mRunning = true;
    mStarted = true;
    computeInterval();
    mNextTick = Clock::now(); // first tick immediately
    sendRealtime(MSG_START);
    std::cout << "[MidiClock] Started at " << mBpm << " BPM ("
              << mIntervalMs << " ms/tick)" << std::endl;
}

void MidiClockGenerator::stop() {
    if (!mRunning) return;
    mRunning = false;
    sendRealtime(MSG_STOP);
    std::cout << "[MidiClock] Stopped" << std::endl;
}

void MidiClockGenerator::resume() {
    if (mRunning) return;
    if (!mStarted) {
        // MIDI protocol: CONTINUE (0xFB) is only valid after a prior START (0xFA).
        start(mBpm);
        return;
    }
    mRunning  = true;
    mNextTick = Clock::now();
    sendRealtime(MSG_CONTINUE);
    std::cout << "[MidiClock] Continued at " << mBpm << " BPM" << std::endl;
}

void MidiClockGenerator::setBpm(float bpm) {
    if (bpm <= 0.f) return;
    mBpm = std::clamp(bpm, 1.f, 999.f);
    computeInterval();
}

void MidiClockGenerator::update() {
    if (!mRunning) return;

    auto now = Clock::now();
    using namespace std::chrono;

    // Send all ticks that are due — drain backlog if frames are slow
    int maxTicksPerFrame = 32; // safety cap: max 32 ticks emitted in one frame
    while (maxTicksPerFrame-- > 0 && now >= mNextTick) {
        sendRealtime(MSG_CLOCK);
        // Advance precisely by interval (avoids drift accumulation)
        mNextTick += duration_cast<Clock::duration>(
            duration<double, std::milli>(mIntervalMs));
    }
}

} // namespace bbfx
