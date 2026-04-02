#include "ViewportCameraController.h"
#include <OgreMath.h>
#include <SDL3/SDL_keyboard.h>
#include <cmath>
#include <algorithm>

namespace bbfx {

static constexpr float kOrbitSensitivity = 0.3f;
static constexpr float kPanSensitivity   = 0.05f;
static constexpr float kZoomSpeed        = 3.0f;
static constexpr float kFpsSensitivity   = 0.15f;
static constexpr float kMinDistance       = 1.0f;

ViewportCameraController::ViewportCameraController(Ogre::SceneManager* sm, Ogre::Camera* cam)
    : mCamera(cam), mScene(sm)
{
    mCamNode = mCamera->getParentSceneNode();
    if (!mCamNode) {
        mCamNode = mScene->getRootSceneNode()->createChildSceneNode("editor_cam_node");
        mCamNode->attachObject(mCamera);
    }

    // Save initial state as default
    auto pos = mCamNode->getPosition();
    // Derive orbit params from current camera position
    mOrbitDistance = pos.length();
    if (mOrbitDistance < kMinDistance) mOrbitDistance = 50.0f;
    mOrbitYaw   = Ogre::Math::ATan2(pos.x, pos.z).valueDegrees();
    mOrbitPitch = Ogre::Math::ASin(std::clamp(pos.y / mOrbitDistance, -1.0f, 1.0f)).valueDegrees();
    mOrbitCenter = Ogre::Vector3::ZERO;

    mDefaultYaw      = mOrbitYaw;
    mDefaultPitch    = mOrbitPitch;
    mDefaultDistance  = mOrbitDistance;
    mDefaultCenter   = mOrbitCenter;

    applyOrbit();
}

void ViewportCameraController::handleMouseMove(float dx, float dy,
                                                bool leftDown, bool rightDown,
                                                bool middleDown, bool altDown)
{
    if (mMode != Mode::Editor) return;

    if (rightDown && !altDown) {
        // FPS look mode — sensitivity scales with move speed
        // so fast strafing doesn't make it impossible to track targets
        float speedFactor = 1.0f + std::log2(std::max(mMoveSpeed, 1.0f)) * 0.3f;
        float sens = kFpsSensitivity * speedFactor;
        mOrbitYaw   -= dx * sens;
        mOrbitPitch -= dy * sens;
        mOrbitPitch  = std::clamp(mOrbitPitch, -89.0f, 89.0f);
        Ogre::Quaternion qy(Ogre::Degree(mOrbitYaw),   Ogre::Vector3::UNIT_Y);
        Ogre::Quaternion qp(Ogre::Degree(mOrbitPitch), Ogre::Vector3::UNIT_X);
        mCamNode->setOrientation(qy * qp);
        // Sync orbit center to stay in front
        auto fwd = mCamNode->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Z;
        mOrbitCenter = mCamNode->getPosition() + fwd * mOrbitDistance;
    }
    else if (altDown && leftDown) {
        // Orbit mode
        mOrbitYaw   += dx * kOrbitSensitivity;
        mOrbitPitch -= dy * kOrbitSensitivity;
        mOrbitPitch  = std::clamp(mOrbitPitch, -89.0f, 89.0f);
        applyOrbit();
    }
    else if (altDown && middleDown) {
        // Pan mode
        float panScale = mOrbitDistance * kPanSensitivity * 0.01f;
        Ogre::Vector3 right = mCamNode->getOrientation() * Ogre::Vector3::UNIT_X;
        Ogre::Vector3 up    = mCamNode->getOrientation() * Ogre::Vector3::UNIT_Y;
        mOrbitCenter -= right * dx * panScale;
        mOrbitCenter += up    * dy * panScale;
        applyOrbit();
    }
}

void ViewportCameraController::handleMouseWheel(float delta, float /*vpX*/, float /*vpY*/)
{
    if (mMode != Mode::Editor) return;

    // Zoom — speed proportional to distance
    float zoomAmount = delta * kZoomSpeed * (mOrbitDistance * 0.1f);
    mOrbitDistance -= zoomAmount;
    mOrbitDistance = std::max(mOrbitDistance, kMinDistance);
    applyOrbit();
}

void ViewportCameraController::handleKeyboard(float dt, bool /*lockOnTarget*/)
{
    if (mMode != Mode::Editor) return;

    const bool* state = SDL_GetKeyboardState(nullptr);
    if (!state) return;

    Ogre::Vector3 move = Ogre::Vector3::ZERO;
    auto fwd   = mCamNode->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Z;
    auto right = mCamNode->getOrientation() * Ogre::Vector3::UNIT_X;

    if (state[SDL_SCANCODE_W])   move += fwd;
    if (state[SDL_SCANCODE_S])   move -= fwd;
    if (state[SDL_SCANCODE_A])   move -= right;
    if (state[SDL_SCANCODE_D])   move += right;
    if (state[SDL_SCANCODE_E] || state[SDL_SCANCODE_SPACE])  move += Ogre::Vector3::UNIT_Y;
    if (state[SDL_SCANCODE_C] || state[SDL_SCANCODE_Q])      move -= Ogre::Vector3::UNIT_Y;

    float speed = mMoveSpeed * std::max(mOrbitDistance * 0.1f, 1.0f);

    if (move.squaredLength() > 0) {
        move.normalise();
        mCamNode->translate(move * speed * dt, Ogre::Node::TS_WORLD);
        auto newFwd = mCamNode->getOrientation() * Ogre::Vector3::NEGATIVE_UNIT_Z;
        mOrbitCenter = mCamNode->getPosition() + newFwd * mOrbitDistance;
    }
}

void ViewportCameraController::handleLockOn(float dt, float mouseDx, float mouseDy, bool entering)
{
    if (mMode != Mode::Editor) return;

    // ── Determine lock target ──
    Ogre::Vector3 target = mLockTarget
        ? mLockTarget->_getDerivedPosition()
        : mOrbitCenter;

    // ── First frame only: derive orbit params from current camera position ──
    // This is done ONCE to ensure seamless transition into lock-on.
    // After this, yaw/pitch/distance are ONLY modified by input, never re-derived.
    if (entering) {
        mOrbitCenter = target;
        Ogre::Vector3 offset = mCamNode->getPosition() - target;
        float dist = offset.length();
        if (dist > kMinDistance) {
            mOrbitDistance = dist;
            mOrbitYaw   = Ogre::Math::ATan2(offset.x, offset.z).valueDegrees();
            mOrbitPitch = Ogre::Math::ASin(std::clamp(offset.y / dist, -1.0f, 1.0f)).valueDegrees();
        }
        // Skip mouse delta on entering frame to avoid jump
        mouseDx = mouseDy = 0;
    } else {
        // Smooth orbit center toward target (for moving objects)
        mOrbitCenter += (target - mOrbitCenter) * std::min(8.0f * dt, 1.0f);
    }

    // ── Mouse: orbit yaw/pitch around target ──
    // Use FPS sensitivity (lower than orbit) since relative mouse mode gives raw deltas
    mOrbitYaw   += mouseDx * kFpsSensitivity;
    mOrbitPitch -= mouseDy * kFpsSensitivity;
    mOrbitPitch  = std::clamp(mOrbitPitch, -89.0f, 89.0f);

    // ── Keyboard ──
    const bool* state = SDL_GetKeyboardState(nullptr);
    if (state) {
        float angularSpeed = 60.0f;
        float dollySpeed = mMoveSpeed * std::max(mOrbitDistance * 0.1f, 1.0f);

        // A/D = orbit yaw (horizontal)
        if (state[SDL_SCANCODE_A])  mOrbitYaw   -= angularSpeed * dt;
        if (state[SDL_SCANCODE_D])  mOrbitYaw   += angularSpeed * dt;
        // E/C = orbit pitch (vertical)
        if (state[SDL_SCANCODE_E] || state[SDL_SCANCODE_SPACE])  mOrbitPitch += angularSpeed * dt;
        if (state[SDL_SCANCODE_C] || state[SDL_SCANCODE_Q])      mOrbitPitch -= angularSpeed * dt;
        mOrbitPitch = std::clamp(mOrbitPitch, -89.0f, 89.0f);
        // W/S = dolly in/out
        if (state[SDL_SCANCODE_W])  mOrbitDistance -= dollySpeed * dt;
        if (state[SDL_SCANCODE_S])  mOrbitDistance += dollySpeed * dt;
        mOrbitDistance = std::max(mOrbitDistance, kMinDistance);
    }

    // ── Apply: position camera on sphere, lookAt center ──
    applyOrbit();
}

void ViewportCameraController::adjustMoveSpeed(float wheelDelta)
{
    // Exponential scaling: each notch multiplies by 1.25x
    // At high speeds the steps feel bigger, at low speeds they feel smaller — natural.
    float notches = std::abs(wheelDelta);
    float factor = std::pow(1.25f, notches);
    if (wheelDelta > 0) mMoveSpeed *= factor;
    else                mMoveSpeed /= factor;
    mMoveSpeed = std::clamp(mMoveSpeed, 0.1f, 1000.0f);
}


void ViewportCameraController::update(float dt)
{
    if (mTransitioning) {
        mTransitionTime += dt;
        float t = std::min(mTransitionTime / mTransitionDuration, 1.0f);
        // Smooth step
        t = t * t * (3.0f - 2.0f * t);
        mOrbitYaw   = mStartYaw   + (mTargetYaw   - mStartYaw)   * t;
        mOrbitPitch = mStartPitch + (mTargetPitch  - mStartPitch) * t;
        applyOrbit();
        if (mTransitionTime >= mTransitionDuration) {
            mTransitioning = false;
        }
    }
}

void ViewportCameraController::focusOn(Ogre::SceneNode* target)
{
    if (!target || mMode != Mode::Editor) return;

    mOrbitCenter = target->_getDerivedPosition();

    // Try to get bounding radius from attached object
    float radius = 10.0f;
    if (target->numAttachedObjects() > 0) {
        auto* obj = target->getAttachedObject(0);
        radius = obj->getBoundingRadius();
        if (radius < 1.0f) radius = 5.0f;
    }
    mOrbitDistance = radius * 2.5f;
    applyOrbit();
}

void ViewportCameraController::resetCamera()
{
    if (mMode != Mode::Editor) return;
    mOrbitYaw      = mDefaultYaw;
    mOrbitPitch    = mDefaultPitch;
    mOrbitDistance  = mDefaultDistance;
    mOrbitCenter   = mDefaultCenter;
    mTransitioning = false;
    applyOrbit();
}

void ViewportCameraController::setPresetView(int numpadKey, bool ctrlDown)
{
    if (mMode != Mode::Editor) return;

    mStartYaw   = mOrbitYaw;
    mStartPitch = mOrbitPitch;

    switch (numpadKey) {
    case 1: // Front / Back
        mTargetYaw   = ctrlDown ? 180.0f : 0.0f;
        mTargetPitch = 0.0f;
        break;
    case 3: // Right / Left
        mTargetYaw   = ctrlDown ? -90.0f : 90.0f;
        mTargetPitch = 0.0f;
        break;
    case 7: // Top / Bottom
        mTargetYaw   = 0.0f;
        mTargetPitch = ctrlDown ? -89.0f : 89.0f;
        break;
    default:
        return;
    }

    mTransitionTime = 0.0f;
    mTransitioning  = true;
}

void ViewportCameraController::applyOrbit()
{
    float yawRad   = Ogre::Math::DegreesToRadians(mOrbitYaw);
    float pitchRad = Ogre::Math::DegreesToRadians(mOrbitPitch);

    float cosP = std::cos(pitchRad);
    Ogre::Vector3 offset(
        mOrbitDistance * std::sin(yawRad) * cosP,
        mOrbitDistance * std::sin(pitchRad),
        mOrbitDistance * std::cos(yawRad) * cosP
    );

    mCamNode->setPosition(mOrbitCenter + offset);
    mCamNode->lookAt(mOrbitCenter, Ogre::Node::TS_WORLD);
}

} // namespace bbfx
