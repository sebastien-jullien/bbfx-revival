#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <OgreSceneManager.h>
#include <OgreCamera.h>
namespace bbfx {
class CameraNode : public AnimationNode {
public:
    CameraNode(const std::string& name, Ogre::SceneManager* scene);
    void update() override;
    void cleanup() override;
    std::string getTypeName() const override { return "CameraNode"; }

    /// When true, the editor camera is active and CameraNode should not
    /// modify the camera position/orientation (but still computes values).
    static bool sEditorCameraActive;

private:
    Ogre::SceneManager* mScene;
    Ogre::Camera* mCamera = nullptr;
    Ogre::SceneNode* mDemoCamNode = nullptr; // original demo camera node (not owned)
    Ogre::SceneNode* mOwnNode = nullptr;     // our own orbit node (owned, destroyed on cleanup)
    ParamSpec mSpec;
    float mTime = 0.0f;
    float mOriginalFov = 45.0f;
};
} // namespace bbfx
