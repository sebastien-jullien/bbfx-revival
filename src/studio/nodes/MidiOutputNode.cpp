#include "MidiOutputNode.h"
#include "../../midi/MidiDeviceManager.h"
#include "../../midi/MidiMessage.h"
#include "../../core/AnimationPort.h"

namespace bbfx {

MidiOutputNode::MidiOutputNode(const std::string& name) : AnimationNode(name) {
    addInput(new AnimationPort("note", 60.0f));
    addInput(new AnimationPort("velocity", 100.0f));
    addInput(new AnimationPort("cc_number", 0.0f));
    addInput(new AnimationPort("cc_value", 0.0f));
    addInput(new AnimationPort("send_note", 0.0f));  // trigger: 0→1 = note-on
    addInput(new AnimationPort("send_cc", 0.0f));     // trigger: 0→1 = send CC
    addInput(new AnimationPort("led_note", -1.0f));  // LED feedback: note number (-1=off)
    addInput(new AnimationPort("led_velocity", 0.0f)); // LED feedback: velocity (0=off, color)
    addInput(new AnimationPort("dt", 0.016f));

    {
        ParamDef d;
        d.name = "channel"; d.type = ParamType::INT;
        d.intVal = 1; d.minVal = 1; d.maxVal = 16;
        d.label = "Channel";
        mSpec.addParam(d);
    }

    setParamSpec(&mSpec);
}

void MidiOutputNode::update() {
    auto* mgr = MidiDeviceManager::instance();
    if (!mgr) return;

    auto& ins = getInputs();
    int channel = 1;
    auto* chanParam = mSpec.getParam("channel");
    if (chanParam) channel = chanParam->intVal;
    uint8_t chanByte = static_cast<uint8_t>((channel - 1) & 0x0F);

    float sendNote = ins["send_note"]->getValue();
    float sendCC = ins["send_cc"]->getValue();

    // Note on/off detection
    if (sendNote > 0.5f && mLastSendNote <= 0.5f) {
        // Transition 0→1: send note-on
        uint8_t note = static_cast<uint8_t>(ins["note"]->getValue()) & 0x7F;
        uint8_t vel = static_cast<uint8_t>(ins["velocity"]->getValue()) & 0x7F;
        mgr->sendMessage(0, MidiMessage::NoteOn | chanByte, note, vel);
    }
    else if (sendNote <= 0.5f && mLastSendNote > 0.5f) {
        // Transition 1→0: send note-off
        uint8_t note = static_cast<uint8_t>(ins["note"]->getValue()) & 0x7F;
        mgr->sendMessage(0, MidiMessage::NoteOff | chanByte, note, 0);
    }
    mLastSendNote = sendNote;

    // CC send
    if (sendCC > 0.5f && mLastSendCC <= 0.5f) {
        uint8_t ccNum = static_cast<uint8_t>(ins["cc_number"]->getValue()) & 0x7F;
        uint8_t ccVal = static_cast<uint8_t>(ins["cc_value"]->getValue() * 127.0f) & 0x7F;
        mgr->sendMessage(0, MidiMessage::ControlChange | chanByte, ccNum, ccVal);
    }
    mLastSendCC = sendCC;

    // LED feedback (for Launchpad, APC, etc.)
    float ledNote = ins["led_note"]->getValue();
    float ledVel = ins["led_velocity"]->getValue();
    if (ledNote >= 0 && (ledNote != mLastLedNote || ledVel != mLastLedVel)) {
        uint8_t note = static_cast<uint8_t>(ledNote) & 0x7F;
        uint8_t vel = static_cast<uint8_t>(ledVel) & 0x7F;
        mgr->sendMessage(0, MidiMessage::NoteOn | chanByte, note, vel);
        mLastLedNote = ledNote;
        mLastLedVel = ledVel;
    }

    fireUpdate();
}

} // namespace bbfx
