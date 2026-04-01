#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
namespace bbfx {
class TriggerNode : public AnimationNode {
public:
    explicit TriggerNode(const std::string& name);
    void update() override;
    std::string getTypeName() const override { return "TriggerNode"; }
private:
    ParamSpec mSpec;
    float mPrevValue = 0.0f;
    float mHoldTimer = 0.0f;
};
} // namespace bbfx
