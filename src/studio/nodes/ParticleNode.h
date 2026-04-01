#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <OgreSceneManager.h>
#include <OgreParticleSystem.h>
namespace bbfx {
class ParticleNode : public AnimationNode {
public:
    ParticleNode(const std::string& name, Ogre::SceneManager* scene);
    ~ParticleNode() override;
    void update() override;
    void cleanup() override;
    void setEnabled(bool en) override;
    std::string getTypeName() const override { return "ParticleNode"; }
private:
    Ogre::SceneManager* mScene;
    Ogre::SceneNode* mSceneNode = nullptr;
    Ogre::ParticleSystem* mPsys = nullptr;
    ParamSpec mSpec;
    std::string mTemplateName;
};
} // namespace bbfx
