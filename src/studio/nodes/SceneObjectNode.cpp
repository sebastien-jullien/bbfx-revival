#include "SceneObjectNode.h"
#include "../../core/Animator.h"
#include <OgreEntity.h>
#include <OgreSubEntity.h>
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
    // Read material from ParamSpec
    std::string matName = "BaseWhiteNoLighting";
    auto* matParam = mSpec.getParam("material");
    if (matParam && !matParam->stringVal.empty()) matName = matParam->stringVal;
    mCurrentMaterial = matName;

    try {
        std::string entityName = getName() + "_entity_" + id;
        std::string snName = getName() + "_sn_" + id;
        auto* entity = mScene->createEntity(entityName, meshName);
        // Do NOT call entity->setMaterialName() here — let the mesh keep its
        // embedded per-submesh materials (Ogre/Skin, Ogre/Eyes, etc.).
        mMovable = entity;
        ++mEntityVersion;
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
            // Do NOT force material — preserve mesh-embedded materials.
            mMovable = entity;
            ++mEntityVersion;
            if (mSceneNode) mSceneNode->attachObject(entity);
        } catch (...) { mMovable = nullptr; }
    }

    // Check if material changed via ParamSpec
    auto* matParam = mSpec.getParam("material");
    if (matParam && !matParam->stringVal.empty() && matParam->stringVal != mCurrentMaterial && mMovable) {
        mCurrentMaterial = matParam->stringVal;
        auto* entity = dynamic_cast<Ogre::Entity*>(mMovable);
        if (entity) {
            for (unsigned s = 0; s < entity->getNumSubEntities(); ++s)
                entity->getSubEntity(s)->setMaterialName(mCurrentMaterial);
        }
    }

    if (!mSceneNode) return;

    // Periodically refresh which ports are DAG-linked
    if (mDAGPriority && ++mLinkRefreshCounter >= 30) {
        mLinkRefreshCounter = 0;
        onLinkChanged();
    }

    auto& in = getInputs();

    // Effective offsets: when DAG Priority is ON, zero offset on axes with active DAG links.
    Ogre::Vector3 effPos = mOffsetPos;
    Ogre::Vector3 effRot = mOffsetRot;
    Ogre::Vector3 effScl = mOffsetScale;
    if (mDAGPriority) {
        if (mLinkedPosX) effPos.x = 0.0f;
        if (mLinkedPosY) effPos.y = 0.0f;
        if (mLinkedPosZ) effPos.z = 0.0f;
        if (mLinkedRotX) effRot.x = 0.0f;
        if (mLinkedRotY) effRot.y = 0.0f;
        if (mLinkedRotZ) effRot.z = 0.0f;
        if (mLinkedSclX) effScl.x = 1.0f;
        if (mLinkedSclY) effScl.y = 1.0f;
        if (mLinkedSclZ) effScl.z = 1.0f;
    }

    // Position & Scale: always set ABSOLUTE values (DAG + offset).
    // These don't compound because we set, not add.
    // Garde NaN/inf : une valeur de port non-finie (div/0 amont, feedback) corromprait
    // le transform et le culling OGRE — on ignore alors la composante fautive.
    {
        float px = in.at("position.x")->getValue() + effPos.x;
        float py = in.at("position.y")->getValue() + effPos.y;
        float pz = in.at("position.z")->getValue() + effPos.z;
        if (std::isfinite(px) && std::isfinite(py) && std::isfinite(pz))
            mSceneNode->setPosition(px, py, pz);
    }

    {
        float sx = in.at("scale.x")->getValue() * effScl.x;
        float sy = in.at("scale.y")->getValue() * effScl.y;
        float sz = in.at("scale.z")->getValue() * effScl.z;
        if (sx > 0.001f && sy > 0.001f && sz > 0.001f)
            mSceneNode->setScale(sx, sy, sz);
    }

    // Rotation: two modes depending on whether DAG ports drive rotation.
    float dagRx = in.at("rotation.x")->getValue();
    float dagRy = in.at("rotation.y")->getValue();
    float dagRz = in.at("rotation.z")->getValue();
    bool rotFromDAG = mLinkedRotX || mLinkedRotY || mLinkedRotZ ||
        std::abs(dagRx) > 0.001f || std::abs(dagRy) > 0.001f || std::abs(dagRz) > 0.001f;

    if (rotFromDAG && std::isfinite(dagRx) && std::isfinite(dagRy) && std::isfinite(dagRz)) {
        // DAG ports drive rotation — set absolute orientation from DAG + offset
        float rx = dagRx + effRot.x, ry = dagRy + effRot.y, rz = dagRz + effRot.z;
        Ogre::Quaternion qx(Ogre::Degree(rx), Ogre::Vector3::UNIT_X);
        Ogre::Quaternion qy(Ogre::Degree(ry), Ogre::Vector3::UNIT_Y);
        Ogre::Quaternion qz(Ogre::Degree(rz), Ogre::Vector3::UNIT_Z);
        mSceneNode->setOrientation(qy * qx * qz);
    } else if (effRot != Ogre::Vector3::ZERO) {
        // No DAG rotation — Lua script may control via entity link (setOrientation each frame).
        // Compose gizmo offset ON TOP of whatever Lua set. Safe: Lua resets base each frame.
        Ogre::Quaternion current = mSceneNode->getOrientation();
        Ogre::Quaternion qx(Ogre::Degree(effRot.x), Ogre::Vector3::UNIT_X);
        Ogre::Quaternion qy(Ogre::Degree(effRot.y), Ogre::Vector3::UNIT_Y);
        Ogre::Quaternion qz(Ogre::Degree(effRot.z), Ogre::Vector3::UNIT_Z);
        mSceneNode->setOrientation(current * qy * qx * qz);
    }

    bool vis;
    if (mLinkedVis) {
        vis = in.at("visible")->getValue() >= 0.5f;
    } else {
        auto* p = mSpec.getParam("visible");
        vis = p ? p->boolVal : true;
    }
    mSceneNode->setVisible(vis && mEnabled && mUserVisible && !mFxHidden);

    fireUpdate();
}

void SceneObjectNode::onLinkChanged() {
    auto* animator = Animator::instance();
    if (!animator) return;

    // Check each transform port: does it have an incoming edge from an enabled node?
    auto checkPort = [&](const std::string& portName) -> bool {
        auto& inputs = getInputs();
        auto it = inputs.find(portName);
        if (it == inputs.end()) return false;
        auto sources = animator->getSourceNodes(it->second);
        for (auto* src : sources) {
            if (src && src->isEnabled()) return true;
        }
        return false;
    };

    mLinkedPosX = checkPort("position.x");
    mLinkedPosY = checkPort("position.y");
    mLinkedPosZ = checkPort("position.z");
    mLinkedRotX = checkPort("rotation.x");
    mLinkedRotY = checkPort("rotation.y");
    mLinkedRotZ = checkPort("rotation.z");
    mLinkedSclX = checkPort("scale.x");
    mLinkedSclY = checkPort("scale.y");
    mLinkedSclZ = checkPort("scale.z");
    mLinkedVis  = checkPort("visible");

}

bool SceneObjectNode::isNodeVisible() const {
    if (!mEnabled || !mUserVisible) return false;
    if (mLinkedVis) {
        const auto& inputs = getInputs();
        auto it = inputs.find("visible");
        return it != inputs.end() && it->second->getValue() >= 0.5f;
    }
    const auto* p = mSpec.getParam("visible");
    return p ? p->boolVal : true;
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
