#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"

namespace bbfx {

/// DAG node — drives the BlendProfile of an output slot.
/// Inputs: output_id (int), left, right, top, bottom, gamma
/// update() pushes values directly to the OutputManager.
class BlendNode : public AnimationNode {
public:
    explicit BlendNode(const std::string& name);
    void update() override;
    std::string getTypeName() const override { return "BlendNode"; }
private:
    ParamSpec mSpec;
};

} // namespace bbfx
