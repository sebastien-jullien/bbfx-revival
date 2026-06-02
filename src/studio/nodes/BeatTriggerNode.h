#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
namespace bbfx {
class BeatTriggerNode : public AnimationNode {
public:
    explicit BeatTriggerNode(const std::string& name);
    void update() override;
    std::string getTypeName() const override { return "BeatTriggerNode"; }
private:
    ParamSpec mSpec;
    float mLastSubBeat = -1.0f;  // dernière subdivision franchie (beat * multiplicateur)
    float mEnvelope = 0.0f;
    bool  mAttacking = false;    // phase d'attaque en cours (rampe vers intensity)
};
} // namespace bbfx
