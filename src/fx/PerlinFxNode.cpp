#include "PerlinFxNode.h"
#include "../core/Engine.h"
#include <OgreMeshManager.h>
#include <OgreEntity.h>
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
    addOutput(new AnimationPort("mesh_dirty", 0.0f));
}

PerlinFxNode::~PerlinFxNode() = default;

void PerlinFxNode::createDeferredEntity() {
    if (mEntityCreated || mStudioEntityName.empty()) return;
    // Check if clone mesh is ready (deferred _prepareClonedMesh)
    auto cloneMesh = Ogre::MeshManager::getSingleton().getByName(mCloneMeshName);
    if (!cloneMesh) return; // not yet cloned
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

void PerlinFxNode::update() {
    // Create Entity once clone mesh is ready (deferred)
    if (!mEntityCreated) createDeferredEntity();

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
    disable(); // remove FrameListener first
    // Hide the Entity + SceneNode instead of destroying them.
    // Destroying during deferred delete causes OGRE render cache invalidation → segfault.
    if (!mStudioSceneNodeName.empty()) {
        try {
            auto* engine = Engine::instance();
            auto* sceneMgr = engine ? engine->getSceneManager() : nullptr;
            if (sceneMgr && sceneMgr->hasSceneNode(mStudioSceneNodeName)) {
                sceneMgr->getSceneNode(mStudioSceneNodeName)->setVisible(false);
            }
        } catch (...) {}
    }
    // Do NOT destroy mShader or the clone mesh — destroying them releases
    // GPU buffers/MeshPtr that OGRE's render cache still references.
    // The shader is disabled (FrameListener removed) so it won't run.
    // Minor memory leak but prevents segfault in next render pass.
}

} // namespace bbfx
