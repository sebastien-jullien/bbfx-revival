#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/Animator.h"
#include "../../core/ParamSpec.h"
#include <OgreSceneManager.h>
#include <OgreParticleSystem.h>
#include <OgreColourValue.h>
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
    // Colour override state (I-1690)
    bool mColorOverride = false;
    bool mAffectorsRemoved = false;  // colour affectors stripped when override active
    Ogre::ColourValue mOverrideColour = Ogre::ColourValue::White;
    std::string mOrigMaterialName;  // saved before tint material applied
    float mPrevSize = -1.0f;
    float mPrevVelocity = -1.0f;
    float mPrevLifetime = -1.0f;
    // Original template values for multiplier mode
    float mOrigWidth = 1.0f, mOrigHeight = 1.0f;
    float mOrigVelMin = 50.0f, mOrigVelMax = 150.0f;
};
} // namespace bbfx
