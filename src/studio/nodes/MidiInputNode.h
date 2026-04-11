#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"

namespace bbfx {

/// DAG node that receives MIDI messages and exposes them as output ports.
/// CC values → normalized 0.0-1.0 float outputs
/// Notes → gate/velocity/note outputs + configurable triggers
/// Pitch bend → -1.0 to 1.0
class MidiInputNode : public AnimationNode {
public:
    explicit MidiInputNode(const std::string& name);
    void update() override;
    std::string getTypeName() const override { return "MidiInputNode"; }

private:
    ParamSpec mSpec;

    // MIDI clock state (v3.3)
    int mClockCount = 0;            // count of Clock ticks (24 per beat)
    double mLastClockTime = 0.0;    // timestamp of last clock tick
    double mClockBPM = 0.0;         // estimated BPM from clock
    bool mTransportRunning = false;
};

} // namespace bbfx
