#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"

namespace bbfx {

/// DAG node that sends OSC messages when input values change.
class OscOutputNode : public AnimationNode {
public:
    explicit OscOutputNode(const std::string& name);
    void update() override;
    std::string getTypeName() const override { return "OscOutputNode"; }

private:
    ParamSpec mSpec;
    float mLastValues[8] = {};
    int mSendCounter = 0;
};

} // namespace bbfx
