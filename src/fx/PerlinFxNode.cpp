#include "PerlinFxNode.h"
#include "../core/Animator.h"
#include "../studio/nodes/SceneObjectNode.h"
#include "../core/Engine.h"
#include <OgreMeshManager.h>
#include <OgreEntity.h>
#include <OgreSceneManager.h>
#include <iostream>

namespace bbfx {

PerlinFxNode::PerlinFxNode(const string& meshName, const string& cloneName)
    : AnimationNode("PerlinFxNode")
    , mShader(std::make_unique<PerlinVertexShader>(meshName, cloneName))
    , mCloneMeshName(cloneName)
{
    addInput(new AnimationPort("dt", 0.016f));
    addInput(new AnimationPort("displacement", 0.15f));
    addInput(new AnimationPort("density", 4.0f));
    addInput(new AnimationPort("timeDensity", 5.0f));
    addInput(new AnimationPort("enable", 1.0f));
    addInput(new AnimationPort("entity", 0.0f));
    addOutput(new AnimationPort("mesh_dirty", 0.0f));

    ParamDef targetDef;
    targetDef.name = "target_entity";
    targetDef.type = ParamType::STRING;
    mSpec.addParam(targetDef);
    setParamSpec(&mSpec);
}

PerlinFxNode::~PerlinFxNode() = default;

void PerlinFxNode::createDeferredEntity() {
    if (mEntityCreated || mStudioEntityName.empty()) return;
    auto cloneMesh = Ogre::MeshManager::getSingleton().getByName(mCloneMeshName);
    if (!cloneMesh) return;
    auto* engine = Engine::instance();
    auto* sceneMgr = engine ? engine->getSceneManager() : nullptr;
    if (!sceneMgr) return;
    try {
        auto* entity = sceneMgr->createEntity(mStudioEntityName, mCloneMeshName);
        auto* sceneNode = sceneMgr->getRootSceneNode()->createChildSceneNode(mStudioSceneNodeName);
        sceneNode->attachObject(entity);
        mEntityCreated = true;
        std::cout << "[PerlinFxNode] Entity '" << mStudioEntityName << "' created from clone" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[PerlinFxNode] Entity creation failed: " << e.what() << std::endl;
    }
}

void PerlinFxNode::setFxVisible(bool vis) {
    if (mStudioSceneNodeName.empty()) return;
    auto* engine = Engine::instance();
    auto* sceneMgr = engine ? engine->getSceneManager() : nullptr;
    if (!sceneMgr) return;
    try {
        if (sceneMgr->hasSceneNode(mStudioSceneNodeName))
            sceneMgr->getSceneNode(mStudioSceneNodeName)->setVisible(vis);
    } catch (...) {}
}

SceneObjectNode* PerlinFxNode::findTargetSceneObj() {
    if (mTargetNodeName.empty()) return nullptr;
    auto* animator = dynamic_cast<Animator*>(getListener());
    if (!animator) return nullptr;
    auto* targetNode = animator->getRegisteredNode(mTargetNodeName);
    return targetNode ? dynamic_cast<SceneObjectNode*>(targetNode) : nullptr;
}

void PerlinFxNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    // Only start/stop the deformation FrameListener.
    // Mesh visibility is managed by resolveTarget() based on entity link state.
    if (en) {
        if (!mTargetNodeName.empty()) enable();
    } else {
        disable();
    }
}

void PerlinFxNode::resolveTarget() {
    std::string targetName;
    auto* td = mSpec.getParam("target_entity");
    if (td) targetName = td->stringVal;

    // Target removed (link deleted) → hide FX clone, restore original mesh
    if (targetName.empty()) {
        if (!mTargetNodeName.empty()) {
            // Restore original mesh visibility
            auto* sceneObj = findTargetSceneObj();
            if (sceneObj && sceneObj->isEnabled() && sceneObj->getSceneNode())
                sceneObj->getSceneNode()->setVisible(true);
            mTargetNodeName.clear();
            disable();
            setFxVisible(false);
        }
        return;
    }

    // Target changed → detach from old, attach to new
    if (targetName != mTargetNodeName) {
        // Restore old target visibility
        if (!mTargetNodeName.empty()) {
            auto* oldObj = findTargetSceneObj();
            if (oldObj && oldObj->isEnabled() && oldObj->getSceneNode())
                oldObj->getSceneNode()->setVisible(true);
        }
        mTargetNodeName = targetName;
        if (isEnabled()) {
            enable();
            setFxVisible(true);
        }
    }

    // Look up the SceneObjectNode to sync position and check enabled state
    auto* sceneObj = findTargetSceneObj();
    if (!sceneObj || !sceneObj->getSceneNode()) return;

    // If target mesh is disabled by user, hide FX too
    if (!sceneObj->isEnabled()) {
        setFxVisible(false);
        return;
    }

    if (isEnabled()) {
        setFxVisible(true);
        // Hide the original mesh — our clone replaces it visually
        sceneObj->getSceneNode()->setVisible(false);
    } else {
        // FX disabled but linked: keep clone visible (frozen), hide original
        // The clone is frozen in its last deformed state since FrameListener is removed
        setFxVisible(true);
        sceneObj->getSceneNode()->setVisible(false);
    }

    // Position our FX entity at the target's position
    auto* engine = Engine::instance();
    auto* sceneMgr = engine ? engine->getSceneManager() : nullptr;
    if (!sceneMgr || mStudioSceneNodeName.empty()) return;
    try {
        if (sceneMgr->hasSceneNode(mStudioSceneNodeName)) {
            auto* fxSn = sceneMgr->getSceneNode(mStudioSceneNodeName);
            fxSn->setPosition(sceneObj->getSceneNode()->_getDerivedPosition());
            fxSn->setScale(sceneObj->getSceneNode()->_getDerivedScale());
            fxSn->setOrientation(sceneObj->getSceneNode()->_getDerivedOrientation());
        }
    } catch (...) {}
}

void PerlinFxNode::update() {
    // Create Entity once clone mesh is ready (deferred)
    if (!mEntityCreated) createDeferredEntity();

    resolveTarget();

    auto& inputs = getInputs();
    auto& outputs = getOutputs();

    float enableVal = inputs.at("enable")->getValue();
    if (enableVal >= 0.5f) {
        mShader->setDisplacement(inputs.at("displacement")->getValue());
        mShader->setDensity(inputs.at("density")->getValue());
        mShader->setTimeDensity(inputs.at("timeDensity")->getValue());
        outputs.at("mesh_dirty")->setValue(1.0f);
    } else {
        outputs.at("mesh_dirty")->setValue(0.0f);
    }
    fireUpdate();
}

void PerlinFxNode::enable() {
    mShader->enable();
}

void PerlinFxNode::disable() {
    mShader->disable();
}

void PerlinFxNode::cleanup() {
    disable();
    // Restore target mesh visibility before cleanup
    auto* sceneObj = findTargetSceneObj();
    if (sceneObj && sceneObj->isEnabled() && sceneObj->getSceneNode())
        sceneObj->getSceneNode()->setVisible(true);
    // Hide the clone
    if (!mStudioSceneNodeName.empty()) {
        try {
            auto* engine = Engine::instance();
            auto* sceneMgr = engine ? engine->getSceneManager() : nullptr;
            if (sceneMgr && sceneMgr->hasSceneNode(mStudioSceneNodeName)) {
                sceneMgr->getSceneNode(mStudioSceneNodeName)->setVisible(false);
            }
        } catch (...) {}
    }
}

void PerlinFxNode::onLinkChanged() {
    resolveTarget();
}

} // namespace bbfx
