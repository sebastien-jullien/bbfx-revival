#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <OgreSceneManager.h>
namespace bbfx {
class SkyboxNode : public AnimationNode {
public:
    SkyboxNode(const std::string& name, Ogre::SceneManager* scene);
    void update() override;
    std::string getTypeName() const override { return "SkyboxNode"; }
private:
    Ogre::SceneManager* mScene;
    ParamSpec mSpec;
};
} // namespace bbfx
