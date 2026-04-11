#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"

namespace bbfx {

/// DAG node that sends MIDI messages based on input port values.
/// When send_note transitions 0→1, sends note-on; 1→0 sends note-off.
/// When send_cc transitions 0→1, sends CC message.
class MidiOutputNode : public AnimationNode {
public:
    explicit MidiOutputNode(const std::string& name);
    void update() override;
    std::string getTypeName() const override { return "MidiOutputNode"; }

private:
    ParamSpec mSpec;
    float mLastSendNote = 0.0f;
    float mLastSendCC = 0.0f;
    float mLastLedNote = -1.0f;
    float mLastLedVel = -1.0f;
};

} // namespace bbfx
