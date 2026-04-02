#include "CameraNode.h"
#include <cmath>
namespace bbfx {

static int sCamCounter = 0;
bool CameraNode::sEditorCameraActive = true;

CameraNode::CameraNode(const std::string& name, Ogre::SceneManager* scene)
    : AnimationNode(name), mScene(scene) {
    ParamDef modeDef; modeDef.name = "mode"; modeDef.label = "Mode"; modeDef.type = ParamType::ENUM;
    modeDef.stringVal = "orbit"; modeDef.choices = {"orbit", "free", "fixed", "track"};
    mSpec.addParam(modeDef);
    ParamDef fovDef; fovDef.name = "fov"; fovDef.label = "FOV"; fovDef.type = ParamType::FLOAT;
    fovDef.floatVal = 45.0f; fovDef.minVal = 10; fovDef.maxVal = 120;
    mSpec.addParam(fovDef);
    ParamDef radDef; radDef.name = "orbit_radius"; radDef.label = "Radius"; radDef.type = ParamType::FLOAT;
    radDef.floatVal = 20.0f; radDef.minVal = 1; radDef.maxVal = 200;
    mSpec.addParam(radDef);
    ParamDef spdDef; spdDef.name = "orbit_speed"; spdDef.label = "Speed"; spdDef.type = ParamType::FLOAT;
    spdDef.floatVal = 0.5f; spdDef.minVal = 0; spdDef.maxVal = 5;
    mSpec.addParam(spdDef);
    ParamDef hDef; hDef.name = "orbit_height"; hDef.label = "Height"; hDef.type = ParamType::FLOAT;
    hDef.floatVal = 5.0f; hDef.minVal = -50; hDef.maxVal = 50;
    mSpec.addParam(hDef);
    setParamSpec(&mSpec);

    addInput(new AnimationPort("dt", 0.016f));
    addInput(new AnimationPort("fov", 45.0f));
    addInput(new AnimationPort("orbit_radius", 20.0f));
    addInput(new AnimationPort("orbit_speed", 0.5f));
    addInput(new AnimationPort("orbit_height", 5.0f));

    if (mScene) {
        try {
            // Save reference to the original demo camera viewport
            mCamera = mScene->getCamera("MainCamera");
            mDemoCamNode = mCamera->getParentSceneNode();
            mOriginalFov = mCamera->getFOVy().valueDegrees();

            // Create our OWN SceneNode for orbiting — don't touch the demo node
            std::string id = std::to_string(++sCamCounter);
            mOwnNode = mScene->getRootSceneNode()->createChildSceneNode("camctrl_" + id);
        } catch (...) {}
    }
}

void CameraNode::cleanup() {
    // Detach camera from our orbit node and reattach to demo node
    if (mCamera && mOwnNode && mCamera->getParentSceneNode() == mOwnNode) {
        mOwnNode->detachObject(mCamera);
    }
    if (mCamera && mDemoCamNode) {
        try { mDemoCamNode->attachObject(mCamera); } catch (...) {}
        mCamera->setFOVy(Ogre::Degree(mOriginalFov));
    }
    // Destroy our own scene node
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

    float radius = in.at("orbit_radius")->getValue();
    float speed = in.at("orbit_speed")->getValue();
    float height = in.at("orbit_height")->getValue();
    float fov = in.at("fov")->getValue();

    // Read mode from ParamSpec
    std::string mode = "orbit";
    auto* mp = mSpec.getParam("mode");
    if (mp && !mp->stringVal.empty()) mode = mp->stringVal;

    // Compute orbit values (always, for DAG consistency)
    float angle = mTime * speed;
    float x = radius * std::sin(angle);
    float z = radius * std::cos(angle);
    mOwnNode->setPosition(x, height, z);
    mOwnNode->lookAt(Ogre::Vector3(0, 0, 0), Ogre::Node::TS_WORLD);

    // Only attach camera and modify FOV when editor camera is NOT active
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

} // namespace bbfx
