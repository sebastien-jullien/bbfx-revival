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
    Ogre::SceneNode* mDemoCamNode = nullptr;
    Ogre::SceneNode* mOwnNode = nullptr;
    ParamSpec mSpec;
    float mTime = 0.0f;
    float mOriginalFov = 45.0f;

    // Fly-through state
    float mFlyPos = 0.0f;
    float mFlyDir = 1.0f;

    // Shake state
    float mShakeDecay = 0.0f;

    // Dolly zoom state
    float mDollyDist = 0.0f;

    // Transition state
    bool mTransitioning = false;
    float mTransitionElapsed = 0.0f;
    float mTransitionDuration = 1.0f;
    Ogre::Vector3 mTransitionStartPos;
    Ogre::Vector3 mTransitionEndPos;
    Ogre::Quaternion mTransitionStartOri;
    Ogre::Quaternion mTransitionEndOri;
    float mTransitionStartFov = 45.0f;
    float mTransitionEndFov = 45.0f;

    void updateOrbit(float dt, float radius, float speed, float height, float fov);
    void updateFlyThrough(float dt, float speed, int axis);
    void updateShake(float dt, float intensity);
    void updateDollyZoom(float dt, float speed, float targetDist);
    void updateTrack(float dt, float damping);
    void updateCrane(float dt, float amplitude, float speed);
    void updateTransition(float dt);
};
} // namespace bbfx
