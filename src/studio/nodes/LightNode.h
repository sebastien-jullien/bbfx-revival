#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <OgreSceneManager.h>
#include <OgreLight.h>

namespace bbfx {

class LightNode : public AnimationNode {
public:
    LightNode(const std::string& name, Ogre::SceneManager* scene);
    ~LightNode() override;
    void update() override;
    void cleanup() override;
    void setEnabled(bool en) override;
    std::string getTypeName() const override { return "LightNode"; }
    Ogre::SceneNode* getSceneNode() const { return mSceneNode; }
private:
    Ogre::SceneManager* mScene;
    Ogre::SceneNode* mSceneNode = nullptr;
    Ogre::Light* mLight = nullptr;
    ParamSpec mSpec;
};

} // namespace bbfx
