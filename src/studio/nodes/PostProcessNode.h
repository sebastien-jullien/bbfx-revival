#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include "../../fx/PostProcessStack.h"
#include <string>

namespace bbfx {

/// DAG node that adds/controls a post-process effect in the PostProcessStack.
/// Creating a PostProcessNode adds an effect; deleting it removes it.
/// The node's ParamSpec drives which effect is selected and its parameters.
class PostProcessNode : public AnimationNode {
public:
    PostProcessNode(const std::string& name, PostProcessStack* stack);
    ~PostProcessNode() override;

    void update() override;
    void cleanup() override;
    std::string getTypeName() const override { return "PostProcessNode"; }

    void setEnabled(bool en) override;

private:
    PostProcessStack* mStack;
    ParamSpec mSpec;
    std::string mEffectName;       ///< Current effect name in the stack (unique per node)
    std::string mCurrentMaterial;  ///< Currently selected material
    bool mRegistered = false;      ///< Whether the effect is in the stack
};

} // namespace bbfx
