#pragma once
#include <OgreSceneManager.h>
#include <OgreCamera.h>
#include <OgreSceneNode.h>
#include <OgreVector.h>
#include <SDL3/SDL_scancode.h>

namespace bbfx {

class ViewportCameraController {
public:
    enum class Mode { Editor, DAGDriven };

    ViewportCameraController(Ogre::SceneManager* sm, Ogre::Camera* cam);

    /// Called when mouse moves over the viewport.
    /// dx/dy are pixel deltas. Button/modifier states indicate active mode.
    void handleMouseMove(float dx, float dy,
                         bool leftDown, bool rightDown, bool middleDown,
                         bool altDown);

    /// Called when mouse wheel scrolls over the viewport.
    void handleMouseWheel(float delta, float viewportX, float viewportY);

    /// Called each frame in FPS mode — polls SDL keyboard state directly.
    /// lockOnTarget=true forces camera to always look at the lock target.
    void handleKeyboard(float dt, bool lockOnTarget = false);

    /// Called each frame to apply smooth transitions.
    void update(float dt);

    /// Focus orbit on target scene node's bounding box.
    void focusOn(Ogre::SceneNode* target);

    /// Reset camera to initial position.
    void resetCamera();

    /// Switch to a preset view: numpad 1=Front, 3=Right, 7=Top.
    /// ctrlDown inverts the view.
    void setPresetView(int numpadKey, bool ctrlDown);

    void setMode(Mode m) { mMode = m; }
    Mode getMode() const { return mMode; }
    Ogre::Vector3 getFocusPoint() const { return mOrbitCenter; }

    /// Adjust FPS move speed via mouse wheel (scroll up = faster, down = slower).
    void adjustMoveSpeed(float wheelDelta);
    float getMoveSpeed() const { return mMoveSpeed; }

    /// Lock-on mode: mouse orbits around target, W/S dolly, unified single path.
    /// entering=true on the first frame of lock-on (syncs orbit params once).
    void handleLockOn(float dt, float mouseDx, float mouseDy, bool entering);

    /// Set the orbit-lock target to a specific scene node (selected object).
    void setOrbitLockTarget(Ogre::SceneNode* node) { mLockTarget = node; }
    Ogre::SceneNode* getLockTarget() const { return mLockTarget; }

    /// Force the orbit center to a specific world position (e.g. ground plane hit).
    void setLockOnCenter(const Ogre::Vector3& pt) { mOrbitCenter = pt; }

private:
    Ogre::Camera* mCamera;
    Ogre::SceneNode* mCamNode;
    Ogre::SceneManager* mScene;

    Mode mMode = Mode::Editor;

    // Orbit state
    float mOrbitYaw     = 0.0f;
    float mOrbitPitch   = 30.0f;
    float mOrbitDistance = 50.0f;
    Ogre::Vector3 mOrbitCenter{0, 0, 0};

    // FPS state
    float mMoveSpeed = 10.0f;

    // Default (for reset)
    float mDefaultYaw = 0.0f, mDefaultPitch = 30.0f, mDefaultDistance = 50.0f;
    Ogre::Vector3 mDefaultCenter{0, 0, 0};

    // Smooth transition for preset views
    bool  mTransitioning = false;
    float mTransitionTime = 0.0f;
    float mTransitionDuration = 0.3f;
    float mTargetYaw = 0, mTargetPitch = 0;
    float mStartYaw = 0, mStartPitch = 0;

    // Orbit-lock target (selected scene node, or nullptr = use orbit center)
    Ogre::SceneNode* mLockTarget = nullptr;

    void applyOrbit();
};

} // namespace bbfx
