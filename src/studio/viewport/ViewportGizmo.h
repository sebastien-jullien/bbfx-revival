#pragma once
#include <OgreSceneManager.h>
#include <OgreCamera.h>
#include <OgreManualObject.h>
#include <OgreSceneNode.h>
#include <OgreVector.h>
#include <OgreQuaternion.h>
#include <string>

namespace bbfx {

class AnimationNode;

/// 3D transformation gizmo rendered in the viewport.
/// Supports translate, rotate, and scale modes with undo/redo integration.
class ViewportGizmo {
public:
    enum class Tool { None, Translate, Rotate, Scale };
    enum class Space { World, Local };

    ViewportGizmo(Ogre::SceneManager* sm, Ogre::Camera* cam);
    ~ViewportGizmo();

    /// Set the target object + corresponding DAG node for transformation.
    void setTarget(Ogre::SceneNode* node, AnimationNode* dagNode);
    void clearTarget();
    bool hasTarget() const { return mTarget != nullptr; }
    Ogre::SceneNode* getTargetSceneNode() const { return mTarget; }

    void setTool(Tool t);
    Tool getTool() const { return mTool; }
    void setSpace(Space s) { mSpace = s; }
    Space getSpace() const { return mSpace; }
    void setSnap(bool enabled, float gridSize = 1.0f) { mSnap = enabled; mSnapSize = gridSize; }

    /// Input handlers. Return true if the gizmo consumed the event.
    bool handleMouseDown(float vpX, float vpY, int button);
    bool handleMouseMove(float vpX, float vpY, float dx, float dy, bool ctrlDown);
    bool handleMouseUp(float vpX, float vpY, int button);

    /// Update gizmo position/scale each frame.
    void update();

    /// Is a drag currently in progress?
    bool isDragging() const { return mDragging; }

    // ── Keyboard mode (G/R/S) ─────────────────────────────────────────────────
    void startKeyboardTransform(Tool tool);
    void confirmKeyboardTransform();
    void cancelKeyboardTransform();
    void constrainAxis(int axis); // 0=X, 1=Y, 2=Z
    bool isInKeyboardMode() const { return mKeyboardMode; }

    struct TransformState {
        Ogre::Vector3 position{0,0,0};
        Ogre::Vector3 scale{1,1,1};
        Ogre::Quaternion orientation{Ogre::Quaternion::IDENTITY};
    };

private:
    Ogre::SceneManager* mScene;
    Ogre::Camera* mCamera;

    Ogre::SceneNode* mTarget = nullptr;
    AnimationNode* mDAGNode = nullptr;

    Tool  mTool  = Tool::Translate;
    Space mSpace = Space::World;
    bool  mSnap  = false;
    float mSnapSize = 1.0f;

    // Gizmo visual
    Ogre::ManualObject* mGizmoObj = nullptr;
    Ogre::SceneNode* mGizmoNode = nullptr;

    // Drag state
    bool mDragging = false;
    int  mDragAxis = -1;       // 0=X, 1=Y, 2=Z, -1=none
    TransformState mStartState;
    int mHighlightAxis = -1;

    // Keyboard mode
    bool mKeyboardMode = false;
    int  mConstrainedAxis = -1; // -1=free, 0=X, 1=Y, 2=Z

    void buildGizmo();
    void buildTranslateGizmo();
    void buildRotateGizmo();
    void buildScaleGizmo();

    int hitTestAxis(float vpX, float vpY);
    Ogre::Vector3 getAxisVector(int axis) const;
    float projectMouseOnAxis(float dx, float dy, int axis);

    void applyTranslate(float dx, float dy, bool snap);
    void applyRotate(float dx, float dy, bool snap);
    void applyScale(float dx, float dy, bool snap);

    void syncToDAGPorts();
    void commitTransform();

    TransformState captureState() const;
    void restoreState(const TransformState& s);
};

} // namespace bbfx
