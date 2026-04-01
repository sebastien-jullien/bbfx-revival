#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"

namespace bbfx {

class MathNode : public AnimationNode {
public:
    explicit MathNode(const std::string& name);
    void update() override;
    std::string getTypeName() const override { return "MathNode"; }
private:
    ParamSpec mSpec;
};

} // namespace bbfx
