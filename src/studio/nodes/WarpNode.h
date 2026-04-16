#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"

namespace bbfx {

/// DAG node — drives the WarpProfile of an output slot.
/// Inputs: output_id (int), tl_x/tl_y, tr_x/tr_y, bl_x/bl_y, br_x/br_y
/// update() pushes corner values directly to the OutputManager.
class WarpNode : public AnimationNode {
public:
    explicit WarpNode(const std::string& name);
    void update() override;
    std::string getTypeName() const override { return "WarpNode"; }
private:
    ParamSpec mSpec;
};

} // namespace bbfx
