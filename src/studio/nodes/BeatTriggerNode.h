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
    float mLastBeat = -1.0f;
    float mEnvelope = 0.0f;
};
} // namespace bbfx
