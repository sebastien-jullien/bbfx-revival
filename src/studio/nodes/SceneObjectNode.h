#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreEntity.h>

namespace bbfx {

/// Creates a visible 3D object in the OGRE scene.
/// Supports mesh, billboard, light, particle system, and plane types.
/// Position, scale, and rotation are animatable via DAG ports.
class SceneObjectNode : public AnimationNode {
public:
    SceneObjectNode(const std::string& name, Ogre::SceneManager* scene);
    ~SceneObjectNode() override;
    void update() override;
    void cleanup() override;
    void setEnabled(bool en) override;
    void setUserVisible(bool v) override;
    std::string getTypeName() const override { return "SceneObjectNode"; }
    Ogre::SceneNode* getSceneNode() const { return mSceneNode; }
    Ogre::Entity* getEntity() const { return dynamic_cast<Ogre::Entity*>(mMovable); }
    std::string getMeshName() const { return mCurrentMesh; }

private:
    Ogre::SceneManager* mScene;
    Ogre::SceneNode* mSceneNode = nullptr;
    Ogre::MovableObject* mMovable = nullptr;
    ParamSpec mSpec;
    std::string mCurrentMesh;

    void createDefaultObject();
};

} // namespace bbfx
