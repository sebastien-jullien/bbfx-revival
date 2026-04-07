#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/Animator.h"
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
    void setUserVisible(bool v) override;
    void onLinkChanged() override;
    std::string getTypeName() const override { return "ParticleNode"; }
    Ogre::SceneNode* getSceneNode() const { return mSceneNode; }
private:
    void resolveTarget();
    Ogre::SceneManager* mScene;
    Ogre::SceneNode* mSceneNode = nullptr;
    Ogre::ParticleSystem* mPsys = nullptr;
    ParamSpec mSpec;
    std::string mTemplateName;
    std::string mTargetNodeName;
};
} // namespace bbfx
