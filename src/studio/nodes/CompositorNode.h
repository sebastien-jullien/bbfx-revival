#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
namespace bbfx {
class CompositorNode : public AnimationNode {
public:
    CompositorNode(const std::string& name, Ogre::Viewport* viewport);
    ~CompositorNode() override;
    void update() override;
    void cleanup() override;
    std::string getTypeName() const override { return "CompositorNode"; }
private:
    Ogre::Viewport* mViewport;
    std::string mCompositorName;
    bool mEnabled = false;
    ParamSpec mSpec;
};
} // namespace bbfx
