#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
namespace bbfx {
class MixerNode : public AnimationNode {
public:
    explicit MixerNode(const std::string& name);
    void update() override;
    std::string getTypeName() const override { return "MixerNode"; }
private:
    ParamSpec mSpec;
    int mNumInputs = 4;
};
} // namespace bbfx
