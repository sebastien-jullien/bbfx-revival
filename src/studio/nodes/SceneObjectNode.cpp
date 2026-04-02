#include "SceneObjectNode.h"
#include <OgreEntity.h>
#include <OgreMeshManager.h>
#include <cmath>
#include <iostream>

namespace bbfx {

static int sSceneObjCounter = 0;

SceneObjectNode::SceneObjectNode(const std::string& name, Ogre::SceneManager* scene)
    : AnimationNode(name), mScene(scene)
{
    // ParamSpec
    ParamDef meshDef;
    meshDef.name = "mesh_file"; meshDef.label = "Mesh"; meshDef.type = ParamType::MESH;
    meshDef.stringVal = "geosphere4500.mesh";
    mSpec.addParam(meshDef);

    ParamDef matDef;
    matDef.name = "material"; matDef.label = "Material"; matDef.type = ParamType::MATERIAL;
    matDef.stringVal = "BaseWhiteNoLighting";
    mSpec.addParam(matDef);

    ParamDef visDef;
    visDef.name = "visible"; visDef.label = "Visible"; visDef.type = ParamType::BOOL;
    visDef.boolVal = true;
    mSpec.addParam(visDef);

    ParamDef parentDef;
    parentDef.name = "parent_node"; parentDef.label = "Parent"; parentDef.type = ParamType::STRING;
    parentDef.stringVal = "";
    mSpec.addParam(parentDef);

    setParamSpec(&mSpec);

    // Animatable ports
    addInput(new AnimationPort("position.x", 0.0f));
    addInput(new AnimationPort("position.y", 0.0f));
    addInput(new AnimationPort("position.z", 0.0f));
    addInput(new AnimationPort("scale.x", 1.0f));
    addInput(new AnimationPort("scale.y", 1.0f));
    addInput(new AnimationPort("scale.z", 1.0f));
    addInput(new AnimationPort("rotation.x", 0.0f));
    addInput(new AnimationPort("rotation.y", 0.0f));
    addInput(new AnimationPort("rotation.z", 0.0f));
    addInput(new AnimationPort("visible", 1.0f));

    // Entity output — used as visual link anchor for FX nodes
    addOutput(new AnimationPort("entity", 0.0f));

    createDefaultObject();
}

SceneObjectNode::~SceneObjectNode() {
    cleanup();
}

void SceneObjectNode::createDefaultObject() {
    if (!mScene) return;
    std::string id = std::to_string(++sSceneObjCounter);
    std::string meshName = "geosphere4500.mesh";
    auto* meshParam = mSpec.getParam("mesh_file");
    if (meshParam && !meshParam->stringVal.empty()) meshName = meshParam->stringVal;
    mCurrentMesh = meshName;
    try {
        std::string entityName = getName() + "_entity_" + id;
        std::string snName = getName() + "_sn_" + id;
        auto* entity = mScene->createEntity(entityName, meshName);
        mMovable = entity;
        mSceneNode = mScene->getRootSceneNode()->createChildSceneNode(snName);
        mSceneNode->attachObject(entity);
        if (meshName == "geosphere4500.mesh" || meshName == "geosphere8000.mesh")
            mSceneNode->setScale(30.0f, 30.0f, 30.0f);
    } catch (const std::exception& e) {
        std::cerr << "[SceneObjectNode] Failed to create mesh '" << meshName << "': " << e.what() << std::endl;
    }
}

void SceneObjectNode::update() {
    // Check if mesh changed via ParamSpec
    auto* meshParam = mSpec.getParam("mesh_file");
    if (meshParam && !meshParam->stringVal.empty() && meshParam->stringVal != mCurrentMesh && mScene) {
        // Destroy old entity and create new one
        if (mMovable && mSceneNode) {
            mSceneNode->detachObject(mMovable);
            mScene->destroyMovableObject(mMovable);
            mMovable = nullptr;
        }
        mCurrentMesh = meshParam->stringVal;
        std::string id = std::to_string(++sSceneObjCounter);
        try {
            auto* entity = mScene->createEntity("sceneobj_ent_" + id, mCurrentMesh);
            mMovable = entity;
            if (mSceneNode) mSceneNode->attachObject(entity);
        } catch (...) { mMovable = nullptr; }
    }
    if (!mSceneNode) return;
    auto& in = getInputs();
    float px = in.at("position.x")->getValue();
    float py = in.at("position.y")->getValue();
    float pz = in.at("position.z")->getValue();
    mSceneNode->setPosition(px, py, pz);

    float sx = in.at("scale.x")->getValue();
    float sy = in.at("scale.y")->getValue();
    float sz = in.at("scale.z")->getValue();
    if (sx > 0.001f && sy > 0.001f && sz > 0.001f)
        mSceneNode->setScale(sx, sy, sz);

    float rx = in.at("rotation.x")->getValue();
    float ry = in.at("rotation.y")->getValue();
    float rz = in.at("rotation.z")->getValue();
    // Only override orientation if rotation ports are non-zero
    // (allows external scripts like rotate_head to control orientation)
    if (std::abs(rx) > 0.001f || std::abs(ry) > 0.001f || std::abs(rz) > 0.001f) {
        Ogre::Quaternion qx(Ogre::Degree(rx), Ogre::Vector3::UNIT_X);
        Ogre::Quaternion qy(Ogre::Degree(ry), Ogre::Vector3::UNIT_Y);
        Ogre::Quaternion qz(Ogre::Degree(rz), Ogre::Vector3::UNIT_Z);
        mSceneNode->setOrientation(qy * qx * qz);
    }

    bool vis = in.at("visible")->getValue() >= 0.5f;
    mSceneNode->setVisible(vis && mEnabled && mUserVisible);

    fireUpdate();
}

void SceneObjectNode::setEnabled(bool en) {
    bool wasDisabled = !mEnabled;
    AnimationNode::setEnabled(en);
    if (mSceneNode) mSceneNode->setVisible(en && mUserVisible);
    // Re-apply current port values immediately so rotation/position resume
    if (en && wasDisabled && mSceneNode) update();
}

void SceneObjectNode::setUserVisible(bool v) {
    AnimationNode::setUserVisible(v);
    if (mSceneNode) mSceneNode->setVisible(v && mEnabled);
}

void SceneObjectNode::cleanup() {
    if (mScene && mSceneNode) {
        if (mMovable) mSceneNode->detachObject(mMovable);
        mScene->destroySceneNode(mSceneNode);
        if (mMovable) mScene->destroyMovableObject(mMovable);
        mSceneNode = nullptr;
        mMovable = nullptr;
    }
}

} // namespace bbfx
