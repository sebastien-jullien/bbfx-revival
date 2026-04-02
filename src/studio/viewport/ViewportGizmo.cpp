#include "ViewportGizmo.h"
#include "../commands/TransformCommands.h"
#include "../commands/CommandManager.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../nodes/SceneObjectNode.h"
#include "ViewportPicker.h"
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreMath.h>
#include <cmath>
#include <algorithm>

namespace bbfx {

static constexpr float kGizmoLength = 5.0f;
static constexpr float kHitTolerance = 0.15f; // fraction of gizmo length for hit-test
static constexpr float kMoveSensitivity = 0.05f;
static constexpr float kRotateSensitivity = 1.0f; // degrees per pixel
static constexpr float kScaleSensitivity = 0.01f;

static void ensureGizmoMaterial(const char* name, float r, float g, float b)
{
    auto& mgr = Ogre::MaterialManager::getSingleton();
    if (mgr.resourceExists(name, Ogre::RGN_DEFAULT)) return;
    auto mat = mgr.create(name, Ogre::RGN_DEFAULT);
    auto* pass = mat->getTechnique(0)->getPass(0);
    pass->setLightingEnabled(false);
    pass->setDepthCheckEnabled(false);
    pass->setDiffuse(r, g, b, 1.0f);
    pass->setAmbient(r, g, b);
    pass->setSelfIllumination(r, g, b);
    pass->setCullingMode(Ogre::CULL_NONE);
}

ViewportGizmo::ViewportGizmo(Ogre::SceneManager* sm, Ogre::Camera* cam)
    : mScene(sm), mCamera(cam)
{
    ensureGizmoMaterial("bbfx/gizmo_red",    0.9f, 0.2f, 0.2f);
    ensureGizmoMaterial("bbfx/gizmo_green",  0.2f, 0.9f, 0.2f);
    ensureGizmoMaterial("bbfx/gizmo_blue",   0.2f, 0.2f, 0.9f);
    ensureGizmoMaterial("bbfx/gizmo_yellow", 0.9f, 0.9f, 0.2f);
    ensureGizmoMaterial("bbfx/gizmo_white",  0.9f, 0.9f, 0.9f);

    mGizmoObj = mScene->createManualObject("bbfx_viewport_gizmo");
    mGizmoObj->setDynamic(true);
    mGizmoObj->setQueryFlags(ViewportPicker::GIZMO_QUERY_MASK);
    mGizmoObj->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY);

    mGizmoNode = mScene->getRootSceneNode()->createChildSceneNode("bbfx_gizmo_node");
    mGizmoNode->attachObject(mGizmoObj);
    mGizmoNode->setVisible(false);
}

ViewportGizmo::~ViewportGizmo()
{
    if (mScene) {
        if (mGizmoNode) {
            mGizmoNode->detachAllObjects();
            mScene->destroySceneNode(mGizmoNode);
        }
        if (mGizmoObj) mScene->destroyManualObject(mGizmoObj);
    }
}

void ViewportGizmo::setTarget(Ogre::SceneNode* node, AnimationNode* dagNode)
{
    mTarget = node;
    mDAGNode = dagNode;
    if (mTarget) {
        mGizmoNode->setVisible(true);
        buildGizmo();
    }
}

void ViewportGizmo::clearTarget()
{
    mTarget = nullptr;
    mDAGNode = nullptr;
    mDragging = false;
    mKeyboardMode = false;
    mGizmoNode->setVisible(false);
}

void ViewportGizmo::setTool(Tool t)
{
    if (mTool == t) return;
    mTool = t;
    if (mTarget) buildGizmo();
}

void ViewportGizmo::buildGizmo()
{
    mGizmoObj->clear();
    switch (mTool) {
    case Tool::Translate: buildTranslateGizmo(); break;
    case Tool::Rotate:    buildRotateGizmo();    break;
    case Tool::Scale:     buildScaleGizmo();     break;
    default: break;
    }
}

// ── Build helpers ──────────────────────────────────────────────────────────────

static void addLine(Ogre::ManualObject* mo, Ogre::Vector3 from, Ogre::Vector3 to)
{
    mo->position(from); mo->position(to);
}

static void addCone(Ogre::ManualObject* mo, Ogre::Vector3 base, Ogre::Vector3 tip, int segments = 8)
{
    // Simple cone fan for gizmo arrow head
    Ogre::Vector3 dir = (tip - base).normalisedCopy();
    Ogre::Vector3 perp = dir.perpendicular().normalisedCopy();
    float radius = 0.3f;

    for (int i = 0; i < segments; ++i) {
        float a1 = Ogre::Math::TWO_PI * i / segments;
        float a2 = Ogre::Math::TWO_PI * (i + 1) / segments;
        Ogre::Quaternion q1(Ogre::Radian(a1), dir);
        Ogre::Quaternion q2(Ogre::Radian(a2), dir);
        Ogre::Vector3 p1 = base + q1 * perp * radius;
        Ogre::Vector3 p2 = base + q2 * perp * radius;
        mo->position(tip); mo->position(p1); mo->position(p2);
    }
}

void ViewportGizmo::buildTranslateGizmo()
{
    float len = kGizmoLength;
    float coneStart = len * 0.85f;

    // X axis (red)
    mGizmoObj->begin("bbfx/gizmo_red", Ogre::RenderOperation::OT_LINE_LIST);
    addLine(mGizmoObj, {0,0,0}, {len,0,0});
    mGizmoObj->end();
    mGizmoObj->begin("bbfx/gizmo_red", Ogre::RenderOperation::OT_TRIANGLE_LIST);
    addCone(mGizmoObj, {coneStart,0,0}, {len,0,0});
    mGizmoObj->end();

    // Y axis (green)
    mGizmoObj->begin("bbfx/gizmo_green", Ogre::RenderOperation::OT_LINE_LIST);
    addLine(mGizmoObj, {0,0,0}, {0,len,0});
    mGizmoObj->end();
    mGizmoObj->begin("bbfx/gizmo_green", Ogre::RenderOperation::OT_TRIANGLE_LIST);
    addCone(mGizmoObj, {0,coneStart,0}, {0,len,0});
    mGizmoObj->end();

    // Z axis (blue)
    mGizmoObj->begin("bbfx/gizmo_blue", Ogre::RenderOperation::OT_LINE_LIST);
    addLine(mGizmoObj, {0,0,0}, {0,0,len});
    mGizmoObj->end();
    mGizmoObj->begin("bbfx/gizmo_blue", Ogre::RenderOperation::OT_TRIANGLE_LIST);
    addCone(mGizmoObj, {0,0,coneStart}, {0,0,len});
    mGizmoObj->end();
}

void ViewportGizmo::buildRotateGizmo()
{
    int segments = 48;
    float radius = kGizmoLength;
    auto circle = [&](const char* mat, int plane) {
        mGizmoObj->begin(mat, Ogre::RenderOperation::OT_LINE_STRIP);
        for (int i = 0; i <= segments; ++i) {
            float a = Ogre::Math::TWO_PI * i / segments;
            float c = std::cos(a) * radius, s = std::sin(a) * radius;
            if (plane == 0) mGizmoObj->position(0, c, s);     // YZ = X axis
            else if (plane == 1) mGizmoObj->position(c, 0, s); // XZ = Y axis
            else mGizmoObj->position(c, s, 0);                 // XY = Z axis
        }
        mGizmoObj->end();
    };
    circle("bbfx/gizmo_red",   0);
    circle("bbfx/gizmo_green", 1);
    circle("bbfx/gizmo_blue",  2);
}

void ViewportGizmo::buildScaleGizmo()
{
    float len = kGizmoLength;
    float cubeSize = 0.3f;
    auto cube = [&](const char* mat, Ogre::Vector3 center) {
        // Simple cube as 12 lines
        float s = cubeSize;
        Ogre::Vector3 corners[8];
        for (int i = 0; i < 8; ++i) {
            corners[i] = center + Ogre::Vector3(
                (i & 1) ? s : -s, (i & 2) ? s : -s, (i & 4) ? s : -s);
        }
        mGizmoObj->begin(mat, Ogre::RenderOperation::OT_LINE_LIST);
        int edges[][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
        for (auto& e : edges) addLine(mGizmoObj, corners[e[0]], corners[e[1]]);
        mGizmoObj->end();
    };

    // Lines
    mGizmoObj->begin("bbfx/gizmo_red", Ogre::RenderOperation::OT_LINE_LIST);
    addLine(mGizmoObj, {0,0,0}, {len,0,0});
    mGizmoObj->end();
    mGizmoObj->begin("bbfx/gizmo_green", Ogre::RenderOperation::OT_LINE_LIST);
    addLine(mGizmoObj, {0,0,0}, {0,len,0});
    mGizmoObj->end();
    mGizmoObj->begin("bbfx/gizmo_blue", Ogre::RenderOperation::OT_LINE_LIST);
    addLine(mGizmoObj, {0,0,0}, {0,0,len});
    mGizmoObj->end();

    // End cubes
    cube("bbfx/gizmo_red",   {len, 0, 0});
    cube("bbfx/gizmo_green", {0, len, 0});
    cube("bbfx/gizmo_blue",  {0, 0, len});
    cube("bbfx/gizmo_white", {0, 0, 0}); // center = uniform scale
}

// ── Hit test ───────────────────────────────────────────────────────────────────

Ogre::Vector3 ViewportGizmo::getAxisVector(int axis) const
{
    if (mSpace == Space::Local && mTarget) {
        auto ori = mTarget->getOrientation();
        switch (axis) {
        case 0: return ori * Ogre::Vector3::UNIT_X;
        case 1: return ori * Ogre::Vector3::UNIT_Y;
        case 2: return ori * Ogre::Vector3::UNIT_Z;
        }
    }
    switch (axis) {
    case 0: return Ogre::Vector3::UNIT_X;
    case 1: return Ogre::Vector3::UNIT_Y;
    case 2: return Ogre::Vector3::UNIT_Z;
    }
    return Ogre::Vector3::ZERO;
}

int ViewportGizmo::hitTestAxis(float vpX, float vpY)
{
    if (!mTarget || !mCamera) return -1;

    Ogre::Ray ray = mCamera->getCameraToViewportRay(vpX, vpY);
    Ogre::Vector3 gizmoPos = mTarget->_getDerivedPosition();

    // Scale gizmo size based on distance to camera
    float dist = (mCamera->getDerivedPosition() - gizmoPos).length();
    float scaledLen = kGizmoLength * dist * 0.02f;

    float bestDist = scaledLen * kHitTolerance * 5.0f; // max acceptable distance
    int bestAxis = -1;

    for (int a = 0; a < 3; ++a) {
        Ogre::Vector3 axisDir = getAxisVector(a);
        Ogre::Vector3 axisEnd = gizmoPos + axisDir * scaledLen;

        // Point-to-line distance in 3D
        Ogre::Vector3 axisVec = axisEnd - gizmoPos;
        Ogre::Vector3 toRayOrigin = ray.getOrigin() - gizmoPos;

        // Closest point between ray and axis line segment
        float t1 = axisVec.dotProduct(ray.getDirection());
        float t2 = axisVec.dotProduct(axisVec);
        float t3 = ray.getDirection().dotProduct(ray.getDirection());
        float denom = t2 * t3 - t1 * t1;
        if (std::fabs(denom) < 1e-6f) continue;

        float s = (toRayOrigin.dotProduct(axisVec) * t3 - toRayOrigin.dotProduct(ray.getDirection()) * t1) / denom;
        s = std::clamp(s, 0.0f, 1.0f);

        float u = (s * t1 - toRayOrigin.dotProduct(ray.getDirection())) / t3;
        if (u < 0) continue;

        Ogre::Vector3 closestOnAxis = gizmoPos + axisVec * s;
        Ogre::Vector3 closestOnRay = ray.getOrigin() + ray.getDirection() * u;
        float d = (closestOnAxis - closestOnRay).length();

        if (d < bestDist) {
            bestDist = d;
            bestAxis = a;
        }
    }

    return bestAxis;
}

float ViewportGizmo::projectMouseOnAxis(float dx, float dy, int axis)
{
    if (!mTarget || !mCamera) return 0;

    // Project the axis direction onto screen space and dot with mouse delta
    Ogre::Vector3 gizmoPos = mTarget->_getDerivedPosition();
    Ogre::Vector3 axisDir = getAxisVector(axis);

    auto viewMat = mCamera->getViewMatrix();
    auto projMat = mCamera->getProjectionMatrix();
    auto vp = viewMat * projMat;

    // Project gizmo pos and gizmo pos + axis to screen
    Ogre::Vector4 p0 = vp * Ogre::Vector4(gizmoPos.x, gizmoPos.y, gizmoPos.z, 1);
    Ogre::Vector4 p1 = vp * Ogre::Vector4(gizmoPos.x + axisDir.x,
                                            gizmoPos.y + axisDir.y,
                                            gizmoPos.z + axisDir.z, 1);
    if (std::fabs(p0.w) < 1e-6f || std::fabs(p1.w) < 1e-6f) return 0;

    float sx0 = p0.x / p0.w, sy0 = p0.y / p0.w;
    float sx1 = p1.x / p1.w, sy1 = p1.y / p1.w;

    float screenDx = sx1 - sx0;
    float screenDy = sy1 - sy0;
    float screenLen = std::sqrt(screenDx * screenDx + screenDy * screenDy);
    if (screenLen < 1e-6f) return 0;

    // Normalize screen axis direction
    screenDx /= screenLen;
    screenDy /= screenLen;

    // Project mouse delta onto screen axis (note: dy inverted for screen coords)
    float ndx = dx * 0.002f; // normalize pixel delta
    float ndy = -dy * 0.002f;
    return (ndx * screenDx + ndy * screenDy) / screenLen;
}

// ── Mouse handlers ─────────────────────────────────────────────────────────────

bool ViewportGizmo::handleMouseDown(float vpX, float vpY, int button)
{
    if (!mTarget || button != 0) return false;

    int axis = hitTestAxis(vpX, vpY);
    if (axis < 0) return false;

    mDragging = true;
    mDragAxis = axis;
    mStartState = captureState();
    return true;
}

bool ViewportGizmo::handleMouseMove(float vpX, float vpY, float dx, float dy, bool ctrlDown)
{
    if (!mTarget) return false;

    if (mDragging || mKeyboardMode) {
        bool snap = ctrlDown || mSnap;
        int axis = mKeyboardMode ? mConstrainedAxis : mDragAxis;

        if (axis < 0 && mKeyboardMode) {
            // Free mode — use screen-space heuristic: horizontal=X, vertical=Y
            // Apply to all axes based on mouse direction
            switch (mTool) {
            case Tool::Translate: applyTranslate(dx, dy, snap); break;
            case Tool::Rotate:   applyRotate(dx, dy, snap);     break;
            case Tool::Scale:    applyScale(dx, dy, snap);      break;
            default: break;
            }
        } else if (axis >= 0) {
            switch (mTool) {
            case Tool::Translate: applyTranslate(dx, dy, snap); break;
            case Tool::Rotate:   applyRotate(dx, dy, snap);     break;
            case Tool::Scale:    applyScale(dx, dy, snap);      break;
            default: break;
            }
        }
        syncToDAGPorts();
        return true;
    }

    // Hover highlight
    int axis = hitTestAxis(vpX, vpY);
    mHighlightAxis = axis;
    return false;
}

bool ViewportGizmo::handleMouseUp(float vpX, float vpY, int button)
{
    if (!mDragging || button != 0) return false;

    mDragging = false;
    commitTransform();
    mDragAxis = -1;
    return true;
}

void ViewportGizmo::update()
{
    if (!mTarget || !mGizmoNode) return;

    // Position gizmo at target
    mGizmoNode->setPosition(mTarget->_getDerivedPosition());

    // Scale gizmo to maintain constant screen size
    float dist = (mCamera->getDerivedPosition() - mTarget->_getDerivedPosition()).length();
    float scale = dist * 0.02f;
    mGizmoNode->setScale(scale, scale, scale);

    // Orientation
    if (mSpace == Space::Local) {
        mGizmoNode->setOrientation(mTarget->_getDerivedOrientation());
    } else {
        mGizmoNode->setOrientation(Ogre::Quaternion::IDENTITY);
    }
}

// ── Transform application ──────────────────────────────────────────────────────

void ViewportGizmo::applyTranslate(float dx, float dy, bool snap)
{
    if (!mTarget) return;
    int axis = mKeyboardMode ? mConstrainedAxis : mDragAxis;

    if (axis >= 0) {
        float amount = projectMouseOnAxis(dx, dy, axis);
        float dist = (mCamera->getDerivedPosition() - mTarget->_getDerivedPosition()).length();
        amount *= dist;

        if (snap) amount = std::round(amount / mSnapSize) * mSnapSize;

        Ogre::Vector3 axisVec = getAxisVector(axis);
        mTarget->translate(axisVec * amount, Ogre::Node::TS_WORLD);
    } else {
        // Free translate in camera plane
        float dist = (mCamera->getDerivedPosition() - mTarget->_getDerivedPosition()).length();
        float scale = dist * kMoveSensitivity * 0.01f;
        Ogre::Vector3 right = mCamera->getDerivedOrientation() * Ogre::Vector3::UNIT_X;
        Ogre::Vector3 up    = mCamera->getDerivedOrientation() * Ogre::Vector3::UNIT_Y;
        mTarget->translate(right * dx * scale + up * (-dy * scale), Ogre::Node::TS_WORLD);
    }
}

void ViewportGizmo::applyRotate(float dx, float dy, bool snap)
{
    if (!mTarget) return;
    int axis = mKeyboardMode ? mConstrainedAxis : mDragAxis;

    float angle = dx * kRotateSensitivity;
    if (snap) angle = std::round(angle / 5.0f) * 5.0f;

    if (axis >= 0) {
        Ogre::Vector3 axisVec = getAxisVector(axis);
        mTarget->rotate(Ogre::Quaternion(Ogre::Degree(angle), axisVec), Ogre::Node::TS_WORLD);
    } else {
        // Free rotate around Y axis
        mTarget->rotate(Ogre::Quaternion(Ogre::Degree(angle), Ogre::Vector3::UNIT_Y), Ogre::Node::TS_WORLD);
    }
}

void ViewportGizmo::applyScale(float dx, float dy, bool snap)
{
    if (!mTarget) return;
    int axis = mKeyboardMode ? mConstrainedAxis : mDragAxis;

    float amount = dx * kScaleSensitivity;
    if (snap) amount = std::round(amount / 0.1f) * 0.1f;

    Ogre::Vector3 curScale = mTarget->getScale();

    if (axis >= 0) {
        float s = 1.0f + amount;
        if (axis == 0) curScale.x *= s;
        if (axis == 1) curScale.y *= s;
        if (axis == 2) curScale.z *= s;
    } else {
        // Uniform scale
        float s = 1.0f + amount;
        curScale *= s;
    }

    // Clamp
    curScale.x = std::max(curScale.x, 0.01f);
    curScale.y = std::max(curScale.y, 0.01f);
    curScale.z = std::max(curScale.z, 0.01f);

    mTarget->setScale(curScale);
}

void ViewportGizmo::syncToDAGPorts()
{
    if (!mDAGNode || !mTarget) return;

    auto& inputs = mDAGNode->getInputs();
    auto setPort = [&](const std::string& name, float val) {
        auto it = inputs.find(name);
        if (it != inputs.end()) it->second->setValue(val);
    };

    auto pos = mTarget->getPosition();
    setPort("position.x", pos.x);
    setPort("position.y", pos.y);
    setPort("position.z", pos.z);

    auto scale = mTarget->getScale();
    setPort("scale.x", scale.x);
    setPort("scale.y", scale.y);
    setPort("scale.z", scale.z);

    // Extract Euler angles from quaternion (YXZ order to match SceneObjectNode)
    auto ori = mTarget->getOrientation();
    Ogre::Matrix3 mat;
    ori.ToRotationMatrix(mat);
    Ogre::Radian yaw, pitch, roll;
    mat.ToEulerAnglesYXZ(yaw, pitch, roll);
    setPort("rotation.x", pitch.valueDegrees());
    setPort("rotation.y", yaw.valueDegrees());
    setPort("rotation.z", roll.valueDegrees());
}

void ViewportGizmo::commitTransform()
{
    if (!mDAGNode || !mTarget) return;

    auto cur = captureState();
    auto& s = mStartState;

    // Extract Euler angles from quaternions
    Ogre::Matrix3 matOld, matNew;
    s.orientation.ToRotationMatrix(matOld);
    cur.orientation.ToRotationMatrix(matNew);
    Ogre::Radian oy, op, or_; matOld.ToEulerAnglesYXZ(oy, op, or_);
    Ogre::Radian ny, np, nr;  matNew.ToEulerAnglesYXZ(ny, np, nr);

    auto cmd = std::make_unique<TransformNodeCommand>(
        mDAGNode->getName(), "Transform " + mDAGNode->getName(),
        s.position.x, s.position.y, s.position.z,
        s.scale.x, s.scale.y, s.scale.z,
        op.valueDegrees(), oy.valueDegrees(), or_.valueDegrees(),
        cur.position.x, cur.position.y, cur.position.z,
        cur.scale.x, cur.scale.y, cur.scale.z,
        np.valueDegrees(), ny.valueDegrees(), nr.valueDegrees()
    );

    // Don't re-execute — the transform is already applied visually
    CommandManager::instance().execute(std::move(cmd));
}

ViewportGizmo::TransformState ViewportGizmo::captureState() const
{
    TransformState s;
    if (mTarget) {
        s.position    = mTarget->getPosition();
        s.scale       = mTarget->getScale();
        s.orientation = mTarget->getOrientation();
    }
    return s;
}

void ViewportGizmo::restoreState(const TransformState& s)
{
    if (mTarget) {
        mTarget->setPosition(s.position);
        mTarget->setScale(s.scale);
        mTarget->setOrientation(s.orientation);
        syncToDAGPorts();
    }
}

// ── Keyboard mode (G/R/S) ─────────────────────────────────────────────────────

void ViewportGizmo::startKeyboardTransform(Tool tool)
{
    if (!mTarget) return;
    mTool = tool;
    mKeyboardMode = true;
    mConstrainedAxis = -1; // free mode initially
    mStartState = captureState();
    buildGizmo();
}

void ViewportGizmo::confirmKeyboardTransform()
{
    if (!mKeyboardMode) return;
    mKeyboardMode = false;
    commitTransform();
}

void ViewportGizmo::cancelKeyboardTransform()
{
    if (!mKeyboardMode) return;
    mKeyboardMode = false;
    restoreState(mStartState);
}

void ViewportGizmo::constrainAxis(int axis)
{
    mConstrainedAxis = axis;
}

} // namespace bbfx
