#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <OgreSceneManager.h>
namespace bbfx {
class FogNode : public AnimationNode {
public:
    FogNode(const std::string& name, Ogre::SceneManager* scene);
    void update() override;
    void cleanup() override;
    std::string getTypeName() const override { return "FogNode"; }
private:
    Ogre::SceneManager* mScene;
    ParamSpec mSpec;
};
} // namespace bbfx
