#include "MidiInputNode.h"
#include "../../midi/MidiDeviceManager.h"
#include "../../midi/MidiMessage.h"
#include "../../core/AnimationPort.h"
#include <iostream>

namespace bbfx {

MidiInputNode::MidiInputNode(const std::string& name) : AnimationNode(name) {
    // Generic outputs
    addOutput(new AnimationPort("note", 0.0f));
    addOutput(new AnimationPort("velocity", 0.0f));
    addOutput(new AnimationPort("gate", 0.0f));
    addOutput(new AnimationPort("cc_number", 0.0f));
    addOutput(new AnimationPort("cc_value", 0.0f));
    addOutput(new AnimationPort("pitch_bend", 0.0f));
    addOutput(new AnimationPort("aftertouch", 0.0f));
    addOutput(new AnimationPort("program", 0.0f));
    addOutput(new AnimationPort("bank_msb", 0.0f));
    addOutput(new AnimationPort("bank_lsb", 0.0f));

    // MIDI clock outputs (v3.3)
    addOutput(new AnimationPort("clock_bpm", 0.0f));   // estimated BPM from MIDI clock
    addOutput(new AnimationPort("clock_pulse", 0.0f));  // 1.0 on each clock tick (24ppqn)
    addOutput(new AnimationPort("clock_beat", 0.0f));   // 1.0 on each beat (every 24 clocks)
    addOutput(new AnimationPort("transport", 0.0f));    // 1.0=playing, 0.0=stopped

    // 8 configurable CC outputs
    for (int i = 1; i <= 8; ++i) {
        addOutput(new AnimationPort("cc" + std::to_string(i), 0.0f));
    }

    // 4 configurable note triggers
    for (int i = 1; i <= 4; ++i) {
        addOutput(new AnimationPort("trig" + std::to_string(i), 0.0f));
    }

    // Input port for dt (required by DAG)
    addInput(new AnimationPort("dt", 0.016f));

    // ParamSpec
    {
        ParamDef d;
        d.name = "channel"; d.type = ParamType::INT;
        d.intVal = 0; d.minVal = 0; d.maxVal = 16; // 0 = omni
        d.label = "Channel (0=omni)";
        mSpec.addParam(d);
    }

    // CC assignments (cc1_number through cc8_number)
    for (int i = 1; i <= 8; ++i) {
        ParamDef d;
        d.name = "cc" + std::to_string(i) + "_number";
        d.type = ParamType::INT;
        d.intVal = -1; d.minVal = -1; d.maxVal = 127;
        d.label = "CC" + std::to_string(i) + " Number (-1=off)";
        mSpec.addParam(d);
    }

    // CC mode: absolute (0) or relative/encoder (1)
    {
        ParamDef d;
        d.name = "cc_mode"; d.type = ParamType::INT;
        d.intVal = 0; d.minVal = 0; d.maxVal = 1;
        d.label = "CC Mode (0=abs, 1=relative)";
        mSpec.addParam(d);
    }

    // Note trigger assignments
    for (int i = 1; i <= 4; ++i) {
        ParamDef d;
        d.name = "trig" + std::to_string(i) + "_note";
        d.type = ParamType::INT;
        d.intVal = -1; d.minVal = -1; d.maxVal = 127;
        d.label = "Trig" + std::to_string(i) + " Note (-1=any)";
        mSpec.addParam(d);
    }

    setParamSpec(&mSpec);
}

void MidiInputNode::update() {
    auto* mgr = MidiDeviceManager::instance();
    if (!mgr) return;

    int filterChannel = 0;
    auto* chanParam = mSpec.getParam("channel");
    if (chanParam) filterChannel = chanParam->intVal;

    auto messages = mgr->poll();
    auto& outs = getOutputs();

    // Reset one-frame pulses
    outs["clock_pulse"]->setValue(0.0f);
    outs["clock_beat"]->setValue(0.0f);

    for (auto& msg : messages) {
        // Channel filter
        if (filterChannel > 0 && msg.channel != filterChannel) continue;

        uint8_t type = msg.type();

        if (type == MidiMessage::NoteOn && msg.data2 > 0) {
            outs["note"]->setValue(static_cast<float>(msg.data1));
            outs["velocity"]->setValue(msg.data2 / 127.0f);
            outs["gate"]->setValue(1.0f);

            // Check note triggers
            for (int i = 1; i <= 4; ++i) {
                auto* p = mSpec.getParam("trig" + std::to_string(i) + "_note");
                if (p && (p->intVal == -1 || p->intVal == msg.data1)) {
                    outs["trig" + std::to_string(i)]->setValue(1.0f);
                }
            }
        }
        else if (type == MidiMessage::NoteOff || (type == MidiMessage::NoteOn && msg.data2 == 0)) {
            outs["gate"]->setValue(0.0f);
            outs["velocity"]->setValue(0.0f);

            for (int i = 1; i <= 4; ++i) {
                auto* p = mSpec.getParam("trig" + std::to_string(i) + "_note");
                if (p && (p->intVal == -1 || p->intVal == msg.data1)) {
                    outs["trig" + std::to_string(i)]->setValue(0.0f);
                }
            }
        }
        else if (type == MidiMessage::ControlChange) {
            outs["cc_number"]->setValue(static_cast<float>(msg.data1));

            auto* ccModeParam = mSpec.getParam("cc_mode");
            bool relativeMode = (ccModeParam && ccModeParam->intVal == 1);

            if (relativeMode) {
                // Relative/encoder: data2 < 64 = increment, >= 64 = decrement
                float delta = (msg.data2 < 64) ? (msg.data2 / 127.0f) : -((128 - msg.data2) / 127.0f);
                float cur = outs["cc_value"]->getValue();
                outs["cc_value"]->setValue(std::max(0.0f, std::min(1.0f, cur + delta)));
            } else {
                outs["cc_value"]->setValue(msg.data2 / 127.0f);
            }

            // Route to configured CC outputs
            for (int i = 1; i <= 8; ++i) {
                auto* p = mSpec.getParam("cc" + std::to_string(i) + "_number");
                if (p && p->intVal == msg.data1) {
                    if (relativeMode) {
                        float delta = (msg.data2 < 64) ? (msg.data2 / 127.0f) : -((128 - msg.data2) / 127.0f);
                        float cur = outs["cc" + std::to_string(i)]->getValue();
                        outs["cc" + std::to_string(i)]->setValue(std::max(0.0f, std::min(1.0f, cur + delta)));
                    } else {
                        outs["cc" + std::to_string(i)]->setValue(msg.data2 / 127.0f);
                    }
                }
            }

            // Bank select
            if (msg.data1 == 0) outs["bank_msb"]->setValue(static_cast<float>(msg.data2));
            if (msg.data1 == 32) outs["bank_lsb"]->setValue(static_cast<float>(msg.data2));
        }
        else if (type == MidiMessage::PitchBend) {
            int bend = (msg.data2 << 7) | msg.data1; // 0-16383, center=8192
            outs["pitch_bend"]->setValue((bend - 8192) / 8192.0f); // -1.0 to 1.0
        }
        else if (type == MidiMessage::ChannelAftertouch) {
            outs["aftertouch"]->setValue(msg.data1 / 127.0f);
        }
        else if (type == MidiMessage::ProgramChange) {
            outs["program"]->setValue(static_cast<float>(msg.data1));
        }

        // MIDI Clock messages (system real-time, no channel)
        if (msg.status == MidiMessage::Clock) {
            outs["clock_pulse"]->setValue(1.0f);
            mClockCount++;
            // Estimate BPM from clock interval (24 ppqn)
            if (msg.timestamp > 0.0 && mLastClockTime > 0.0) {
                double interval = msg.timestamp - mLastClockTime;
                if (interval > 0.0 && interval < 1.0) { // sanity: <1 second
                    // 24 clocks per beat → beat duration = interval * 24
                    double beatDuration = interval * 24.0;
                    double bpm = 60.0 / beatDuration;
                    // Smooth with low-pass filter
                    mClockBPM = (mClockBPM > 0.0) ? mClockBPM * 0.9 + bpm * 0.1 : bpm;
                    outs["clock_bpm"]->setValue(static_cast<float>(mClockBPM));
                }
            }
            mLastClockTime = msg.timestamp;
            // Beat pulse every 24 clocks
            if (mClockCount >= 24) {
                outs["clock_beat"]->setValue(1.0f);
                mClockCount = 0;
            }
        }
        else if (msg.status == MidiMessage::Start || msg.status == MidiMessage::Continue) {
            mTransportRunning = true;
            mClockCount = 0;
            outs["transport"]->setValue(1.0f);
        }
        else if (msg.status == MidiMessage::Stop) {
            mTransportRunning = false;
            outs["transport"]->setValue(0.0f);
        }
    }

    fireUpdate();
}

} // namespace bbfx
