#include "CameraNode.h"
#include <cmath>
#include <iostream>
namespace bbfx {

static int sCamCounter = 0;
bool CameraNode::sEditorCameraActive = true;

// Simple Perlin-like noise hash for shake
static float hashF(float x, float y) {
    float h = std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
    return h - std::floor(h);
}

static float noise1D(float t) {
    float i = std::floor(t);
    float f = t - i;
    f = f * f * (3.0f - 2.0f * f);
    return (1.0f - f) * (hashF(i, 0.0f) * 2.0f - 1.0f) +
           f * (hashF(i + 1.0f, 0.0f) * 2.0f - 1.0f);
}

CameraNode::CameraNode(const std::string& name, Ogre::SceneManager* scene)
    : AnimationNode(name), mScene(scene) {
    // Mode enum — extended with all 6 modes
    ParamDef modeDef; modeDef.name = "mode"; modeDef.label = "Mode"; modeDef.type = ParamType::ENUM;
    modeDef.stringVal = "orbit"; modeDef.choices = {"orbit", "fly_through", "shake", "dolly_zoom", "track", "crane"};
    mSpec.addParam(modeDef);

    // Common params
    ParamDef fovDef; fovDef.name = "fov"; fovDef.label = "FOV"; fovDef.type = ParamType::FLOAT;
    fovDef.floatVal = 45.0f; fovDef.minVal = 10; fovDef.maxVal = 120;
    mSpec.addParam(fovDef);

    // Orbit params
    ParamDef radDef; radDef.name = "orbit_radius"; radDef.label = "Radius"; radDef.type = ParamType::FLOAT;
    radDef.floatVal = 20.0f; radDef.minVal = 1; radDef.maxVal = 200;
    mSpec.addParam(radDef);
    ParamDef spdDef; spdDef.name = "orbit_speed"; spdDef.label = "Speed"; spdDef.type = ParamType::FLOAT;
    spdDef.floatVal = 0.5f; spdDef.minVal = 0; spdDef.maxVal = 5;
    mSpec.addParam(spdDef);
    ParamDef hDef; hDef.name = "orbit_height"; hDef.label = "Height"; hDef.type = ParamType::FLOAT;
    hDef.floatVal = 5.0f; hDef.minVal = -50; hDef.maxVal = 50;
    mSpec.addParam(hDef);

    // Fly-through params
    ParamDef flySpdDef; flySpdDef.name = "fly_speed"; flySpdDef.label = "Fly Speed"; flySpdDef.type = ParamType::FLOAT;
    flySpdDef.floatVal = 5.0f; flySpdDef.minVal = 0; flySpdDef.maxVal = 50;
    mSpec.addParam(flySpdDef);
    ParamDef flyAxisDef; flyAxisDef.name = "fly_axis"; flyAxisDef.label = "Fly Axis"; flyAxisDef.type = ParamType::INT;
    flyAxisDef.intVal = 2; flyAxisDef.minVal = 0; flyAxisDef.maxVal = 2;
    mSpec.addParam(flyAxisDef);

    // Shake params
    ParamDef shakeDef; shakeDef.name = "shake_intensity"; shakeDef.label = "Shake"; shakeDef.type = ParamType::FLOAT;
    shakeDef.floatVal = 0.0f; shakeDef.minVal = 0; shakeDef.maxVal = 1;
    mSpec.addParam(shakeDef);

    // Dolly zoom params
    ParamDef dollySpdDef; dollySpdDef.name = "dolly_speed"; dollySpdDef.label = "Dolly Speed"; dollySpdDef.type = ParamType::FLOAT;
    dollySpdDef.floatVal = 2.0f; dollySpdDef.minVal = 0; dollySpdDef.maxVal = 10;
    mSpec.addParam(dollySpdDef);
    ParamDef targetDistDef; targetDistDef.name = "target_distance"; targetDistDef.label = "Target Dist"; targetDistDef.type = ParamType::FLOAT;
    targetDistDef.floatVal = 15.0f; targetDistDef.minVal = 1; targetDistDef.maxVal = 100;
    mSpec.addParam(targetDistDef);

    // Track params
    ParamDef dampDef; dampDef.name = "damping"; dampDef.label = "Damping"; dampDef.type = ParamType::FLOAT;
    dampDef.floatVal = 0.1f; dampDef.minVal = 0; dampDef.maxVal = 1;
    mSpec.addParam(dampDef);

    // Crane params
    ParamDef craneAmpDef; craneAmpDef.name = "crane_amplitude"; craneAmpDef.label = "Crane Amp"; craneAmpDef.type = ParamType::FLOAT;
    craneAmpDef.floatVal = 10.0f; craneAmpDef.minVal = 0; craneAmpDef.maxVal = 50;
    mSpec.addParam(craneAmpDef);
    ParamDef craneSpdDef; craneSpdDef.name = "crane_speed"; craneSpdDef.label = "Crane Speed"; craneSpdDef.type = ParamType::FLOAT;
    craneSpdDef.floatVal = 0.3f; craneSpdDef.minVal = 0; craneSpdDef.maxVal = 3;
    mSpec.addParam(craneSpdDef);

    // Transition params
    ParamDef transDef; transDef.name = "transition_time"; transDef.label = "Trans Time"; transDef.type = ParamType::FLOAT;
    transDef.floatVal = 1.0f; transDef.minVal = 0.1f; transDef.maxVal = 10;
    mSpec.addParam(transDef);

    setParamSpec(&mSpec);

    // DAG input ports
    addInput(new AnimationPort("dt", 0.016f));
    addInput(new AnimationPort("fov", 45.0f));
    addInput(new AnimationPort("orbit_radius", 20.0f));
    addInput(new AnimationPort("orbit_speed", 0.5f));
    addInput(new AnimationPort("orbit_height", 5.0f));
    addInput(new AnimationPort("fly_speed", 5.0f));
    addInput(new AnimationPort("shake_intensity", 0.0f));
    addInput(new AnimationPort("dolly_speed", 2.0f));
    addInput(new AnimationPort("crane_amplitude", 10.0f));
    addInput(new AnimationPort("crane_speed", 0.3f));
    addInput(new AnimationPort("damping", 0.1f));

    if (mScene) {
        try {
            mCamera = mScene->getCamera("MainCamera");
            mDemoCamNode = mCamera->getParentSceneNode();
            mOriginalFov = mCamera->getFOVy().valueDegrees();
            std::string id = std::to_string(++sCamCounter);
            mOwnNode = mScene->getRootSceneNode()->createChildSceneNode("camctrl_" + id);
        } catch (...) {}
    }

    // Init dolly distance
    mDollyDist = 20.0f;
}

void CameraNode::cleanup() {
    if (mCamera && mOwnNode && mCamera->getParentSceneNode() == mOwnNode) {
        mOwnNode->detachObject(mCamera);
    }
    if (mCamera && mDemoCamNode) {
        try { mDemoCamNode->attachObject(mCamera); } catch (...) {}
        mCamera->setFOVy(Ogre::Degree(mOriginalFov));
    }
    if (mScene && mOwnNode) {
        try { mScene->destroySceneNode(mOwnNode); } catch (...) {}
        mOwnNode = nullptr;
    }
    mCamera = nullptr;
    mDemoCamNode = nullptr;
}

void CameraNode::update() {
    if (!mCamera || !mOwnNode) return;
    auto& in = getInputs();
    float dt = in.at("dt")->getValue();
    mTime += dt;

    float fov = in.at("fov")->getValue();

    // Read mode from ParamSpec
    std::string mode = "orbit";
    auto* mp = mSpec.getParam("mode");
    if (mp && !mp->stringVal.empty()) mode = mp->stringVal;

    // D12 — armement d'une transition au changement de mode. Le sous-système
    // updateTransition() existait mais n'était jamais déclenché (`mTransitioning`
    // jamais mis à true) → `transition_time` était un contrôle mort. On capture la
    // pose courante comme point de départ et on blende vers la pose live du nouveau
    // mode pendant `transition_time` secondes.
    if (!mPrevMode.empty() && mode != mPrevMode) {
        auto* ttDef = mSpec.getParam("transition_time");
        float tt = ttDef ? ttDef->floatVal : 0.0f;
        if (tt > 0.001f) {
            mTransitioning = true;
            mTransitionElapsed = 0.0f;
            mTransitionDuration = tt;
            mTransitionStartPos = mOwnNode->getPosition();
            mTransitionStartOri = mOwnNode->getOrientation();
            mTransitionStartFov = fov;
        }
    }
    mPrevMode = mode;

    // Dispatch to mode-specific update
    if (mode == "orbit") {
        updateOrbit(dt, in.at("orbit_radius")->getValue(),
                    in.at("orbit_speed")->getValue(),
                    in.at("orbit_height")->getValue(), fov);
    } else if (mode == "fly_through") {
        auto* axisDef = mSpec.getParam("fly_axis");
        int axis = axisDef ? axisDef->intVal : 2;
        updateFlyThrough(dt, in.at("fly_speed")->getValue(), axis);
    } else if (mode == "shake") {
        // Shake is additive on orbit
        updateOrbit(dt, in.at("orbit_radius")->getValue(),
                    in.at("orbit_speed")->getValue(),
                    in.at("orbit_height")->getValue(), fov);
        updateShake(dt, in.at("shake_intensity")->getValue());
    } else if (mode == "dolly_zoom") {
        auto* tdDef = mSpec.getParam("target_distance");
        float targetDist = tdDef ? tdDef->floatVal : 15.0f;
        updateDollyZoom(dt, in.at("dolly_speed")->getValue(), targetDist);
    } else if (mode == "track") {
        updateTrack(dt, in.at("damping")->getValue());
    } else if (mode == "crane") {
        updateCrane(dt, in.at("crane_amplitude")->getValue(),
                    in.at("crane_speed")->getValue());
    } else {
        // Fallback to orbit
        updateOrbit(dt, in.at("orbit_radius")->getValue(),
                    in.at("orbit_speed")->getValue(),
                    in.at("orbit_height")->getValue(), fov);
    }

    // Handle active transition (overrides position/orientation). Le mode vient de
    // poser mOwnNode sur la cible LIVE → on la capture comme point d'arrivée (elle
    // peut bouger, ex. orbit), puis updateTransition() blende depuis la pose figée.
    if (mTransitioning) {
        mTransitionEndPos = mOwnNode->getPosition();
        mTransitionEndOri = mOwnNode->getOrientation();
        mTransitionEndFov = fov;
        updateTransition(dt);
    }

    // Only attach camera when editor camera is NOT active
    if (!sEditorCameraActive) {
        if (mCamera->getParentSceneNode() != mOwnNode) {
            if (mCamera->getParentSceneNode())
                mCamera->getParentSceneNode()->detachObject(mCamera);
            mOwnNode->attachObject(mCamera);
        }
        mCamera->setFOVy(Ogre::Degree(fov));
    }

    fireUpdate();
}

void CameraNode::updateOrbit(float /*dt*/, float radius, float speed, float height, float /*fov*/) {
    float angle = mTime * speed;
    float x = radius * std::sin(angle);
    float z = radius * std::cos(angle);
    mOwnNode->setPosition(x, height, z);
    mOwnNode->lookAt(Ogre::Vector3(0, 0, 0), Ogre::Node::TS_WORLD);
}

void CameraNode::updateFlyThrough(float dt, float speed, int axis) {
    mFlyPos += dt * speed * mFlyDir;

    // Ping-pong at limits (±100 units)
    const float limit = 100.0f;
    if (mFlyPos > limit) { mFlyPos = limit; mFlyDir = -1.0f; }
    if (mFlyPos < -limit) { mFlyPos = -limit; mFlyDir = 1.0f; }

    Ogre::Vector3 pos(0, 5, 0);
    Ogre::Vector3 dir(0, 0, 0);
    if (axis == 0) { pos.x = mFlyPos; dir = Ogre::Vector3(mFlyDir, 0, 0); }
    else if (axis == 1) { pos.y = mFlyPos; dir = Ogre::Vector3(0, mFlyDir, 0); }
    else { pos.z = mFlyPos; dir = Ogre::Vector3(0, 0, mFlyDir); }

    mOwnNode->setPosition(pos);
    mOwnNode->setDirection(dir, Ogre::Node::TS_WORLD);
}

void CameraNode::updateShake(float dt, float intensity) {
    if (intensity < 0.001f) return;

    // Exponential decay when intensity is active
    mShakeDecay = intensity;

    Ogre::Vector3 pos = mOwnNode->getPosition();
    float offsetX = noise1D(mTime * 15.0f) * mShakeDecay * 2.0f;
    float offsetY = noise1D(mTime * 15.0f + 100.0f) * mShakeDecay * 2.0f;
    float offsetZ = noise1D(mTime * 15.0f + 200.0f) * mShakeDecay * 2.0f;
    pos += Ogre::Vector3(offsetX, offsetY, offsetZ);
    mOwnNode->setPosition(pos);
}

void CameraNode::updateDollyZoom(float dt, float speed, float targetDist) {
    // Oscillate distance around target
    mDollyDist = targetDist + std::sin(mTime * speed) * targetDist * 0.5f;
    float dist = std::max(mDollyDist, 1.0f);

    // Maintain apparent size: FOV changes inversely with distance
    // FOV = 2 * atan(size / (2 * dist))
    // At target_distance, FOV = original. As dist changes, FOV compensates.
    float baseFov = 45.0f;
    float fovRad = 2.0f * std::atan(targetDist * std::tan(baseFov * 0.5f * 3.14159f / 180.0f) / dist);
    float fov = fovRad * 180.0f / 3.14159f;
    fov = std::max(5.0f, std::min(150.0f, fov));

    mOwnNode->setPosition(0, 5, dist);
    mOwnNode->lookAt(Ogre::Vector3(0, 0, 0), Ogre::Node::TS_WORLD);

    if (!sEditorCameraActive && mCamera) {
        mCamera->setFOVy(Ogre::Degree(fov));
    }
}

void CameraNode::updateTrack(float dt, float damping) {
    // Find first SceneObjectNode's entity position to track
    Ogre::Vector3 targetPos(0, 0, 0);

    // Look for any visible entity to track
    auto it = mScene->getMovableObjectIterator("Entity");
    if (it.hasMoreElements()) {
        auto* movable = it.getNext();
        if (movable && movable->getParentSceneNode()) {
            targetPos = movable->getParentSceneNode()->getPosition();
        }
    }

    // Offset behind and above
    Ogre::Vector3 desiredPos = targetPos + Ogre::Vector3(0, 8, 20);
    Ogre::Vector3 currentPos = mOwnNode->getPosition();

    // Lerp towards desired position
    float t = 1.0f - std::pow(damping, dt * 60.0f);
    Ogre::Vector3 newPos = currentPos + (desiredPos - currentPos) * t;
    mOwnNode->setPosition(newPos);
    mOwnNode->lookAt(targetPos, Ogre::Node::TS_WORLD);
}

void CameraNode::updateCrane(float dt, float amplitude, float speed) {
    float angle = mTime * speed;
    float height = amplitude * std::sin(angle * 0.7f);
    float radius = 25.0f;
    float x = radius * std::sin(angle);
    float z = radius * std::cos(angle);
    mOwnNode->setPosition(x, height + 10.0f, z);
    mOwnNode->lookAt(Ogre::Vector3(0, 0, 0), Ogre::Node::TS_WORLD);
}

void CameraNode::updateTransition(float dt) {
    mTransitionElapsed += dt;
    float t = std::min(mTransitionElapsed / mTransitionDuration, 1.0f);
    // Smoothstep
    t = t * t * (3.0f - 2.0f * t);

    Ogre::Vector3 pos = mTransitionStartPos + (mTransitionEndPos - mTransitionStartPos) * t;
    Ogre::Quaternion ori = Ogre::Quaternion::Slerp(t, mTransitionStartOri, mTransitionEndOri, true);
    mOwnNode->setPosition(pos);
    mOwnNode->setOrientation(ori);

    if (!sEditorCameraActive && mCamera) {
        float fov = mTransitionStartFov + (mTransitionEndFov - mTransitionStartFov) * t;
        mCamera->setFOVy(Ogre::Degree(fov));
    }

    if (mTransitionElapsed >= mTransitionDuration) {
        mTransitioning = false;
    }
}

} // namespace bbfx
