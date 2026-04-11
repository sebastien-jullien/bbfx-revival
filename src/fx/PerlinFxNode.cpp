#include "PerlinFxNode.h"
#include "../core/Animator.h"
#include "../studio/nodes/SceneObjectNode.h"
#include "../core/Engine.h"
#include <OgreMeshManager.h>
#include <OgreEntity.h>
#include <OgreSceneManager.h>
#include <iostream>
#include <algorithm>

namespace bbfx {

static int sCloneCounter = 0;

PerlinFxNode::PerlinFxNode(const string& defaultMesh, const string& clonePrefix, const std::string& nodeName)
    : AnimationNode(nodeName)
    , mClonePrefix(clonePrefix)
    , mDefaultMesh(defaultMesh)
{
    addInput(new AnimationPort("dt", 0.016f));
    addInput(new AnimationPort("displacement", 0.15f));
    addInput(new AnimationPort("density", 4.0f));
    addInput(new AnimationPort("timeDensity", 5.0f));
    addInput(new AnimationPort("enable", 1.0f));
    addInput(new AnimationPort("entity", 0.0f, true));  // multiLink
    addOutput(new AnimationPort("mesh_dirty", 0.0f));

    ParamDef targetDef;
    targetDef.name = "target_entity";
    targetDef.type = ParamType::STRING;
    mSpec.addParam(targetDef);
    setParamSpec(&mSpec);
}

PerlinFxNode::~PerlinFxNode() = default;

SceneObjectNode* PerlinFxNode::findTargetSceneObj(const std::string& targetName) {
    if (targetName.empty()) return nullptr;
    auto* animator = Animator::instance();
    if (!animator) return nullptr;
    auto* targetNode = animator->getRegisteredNode(targetName);
    return targetNode ? dynamic_cast<SceneObjectNode*>(targetNode) : nullptr;
}

void PerlinFxNode::addCloneForTarget(const std::string& targetName) {
    if (mClones.count(targetName)) return;

    // Determine source mesh from the target SceneObjectNode
    std::string sourceMesh = mDefaultMesh;
    auto* sceneObj = findTargetSceneObj(targetName);
    if (sceneObj) {
        auto* spec = sceneObj->getParamSpec();
        if (spec) {
            auto* meshParam = spec->getParam("mesh_file");
            if (meshParam && !meshParam->stringVal.empty())
                sourceMesh = meshParam->stringVal;
        }
    }

    // Create unique clone names
    std::string suffix = "_" + std::to_string(sCloneCounter++);
    std::string cloneName = mClonePrefix + suffix;
    std::string entName = "perlin_ent" + suffix;
    std::string snName = "perlin_sn" + suffix;

    FxClone clone;
    clone.cloneMeshName = cloneName;
    clone.entityName = entName;
    clone.sceneNodeName = snName;
    clone.shader = std::make_unique<PerlinVertexShader>(sourceMesh, cloneName);
    clone.shader->enable();
    clone.entityCreated = false;

    mClones[targetName] = std::move(clone);
    std::cout << "[PerlinFxNode] Clone created for target '" << targetName
              << "' (mesh: " << sourceMesh << ")" << std::endl;
}

void PerlinFxNode::removeClone(const std::string& targetName) {
    auto it = mClones.find(targetName);
    if (it == mClones.end()) return;

    auto& clone = it->second;
    clone.shader->disable();

    // Restore target visibility
    auto* sceneObj = findTargetSceneObj(targetName);
    if (sceneObj && sceneObj->isEnabled() && sceneObj->getSceneNode())
        sceneObj->getSceneNode()->setVisible(true);

    // Remove OGRE objects
    auto* engine = Engine::instance();
    auto* sceneMgr = engine ? engine->getSceneManager() : nullptr;
    if (sceneMgr && clone.entityCreated) {
        try {
            if (sceneMgr->hasSceneNode(clone.sceneNodeName)) {
                auto* sn = sceneMgr->getSceneNode(clone.sceneNodeName);
                sn->detachAllObjects();
                sceneMgr->destroySceneNode(sn);
            }
            if (sceneMgr->hasEntity(clone.entityName)) {
                sceneMgr->destroyEntity(clone.entityName);
            }
        } catch (...) {}
    }

    mClones.erase(it);
    std::cout << "[PerlinFxNode] Clone removed for target '" << targetName << "'" << std::endl;
}

void PerlinFxNode::setCloneVisible(const std::string& targetName, bool vis) {
    auto it = mClones.find(targetName);
    if (it == mClones.end()) return;
    auto* engine = Engine::instance();
    auto* sceneMgr = engine ? engine->getSceneManager() : nullptr;
    if (!sceneMgr) return;
    try {
        if (sceneMgr->hasSceneNode(it->second.sceneNodeName))
            sceneMgr->getSceneNode(it->second.sceneNodeName)->setVisible(vis);
    } catch (...) {}
}

void PerlinFxNode::createDeferredEntities() {
    auto* engine = Engine::instance();
    auto* sceneMgr = engine ? engine->getSceneManager() : nullptr;
    if (!sceneMgr) return;

    for (auto& [targetName, clone] : mClones) {
        if (clone.entityCreated) continue;
        auto cloneMesh = Ogre::MeshManager::getSingleton().getByName(clone.cloneMeshName);
        if (!cloneMesh) continue;
        try {
            auto* entity = sceneMgr->createEntity(clone.entityName, clone.cloneMeshName);
            auto* sceneNode = sceneMgr->getRootSceneNode()->createChildSceneNode(clone.sceneNodeName);
            sceneNode->attachObject(entity);
            clone.entityCreated = true;
        } catch (const std::exception& e) {
            std::cerr << "[PerlinFxNode] Entity creation failed: " << e.what() << std::endl;
        }
    }
}

void PerlinFxNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (en) {
        for (auto& [name, clone] : mClones) clone.shader->enable();
    } else {
        for (auto& [name, clone] : mClones) clone.shader->disable();
    }
}

void PerlinFxNode::resolveTargets() {
    // Read target list from the DAG graph
    std::vector<std::string> newTargets;
    auto* animator = Animator::instance();
    if (animator) {
        auto& inputs = getInputs();
        auto it = inputs.find("entity");
        if (it != inputs.end()) {
            auto sources = animator->getSourceNodes(it->second);
            for (auto* srcNode : sources) {
                if (srcNode && !srcNode->getName().empty())
                    newTargets.push_back(srcNode->getName());
            }
        }
    }

    // Detect removed targets
    for (auto& old : mTargetNodeNames) {
        if (std::find(newTargets.begin(), newTargets.end(), old) == newTargets.end()) {
            removeClone(old);
        }
    }

    // Detect added targets
    for (auto& nt : newTargets) {
        if (std::find(mTargetNodeNames.begin(), mTargetNodeNames.end(), nt) == mTargetNodeNames.end()) {
            addCloneForTarget(nt);
        }
    }

    mTargetNodeNames = newTargets;

    // Sync each clone with its target
    auto* engine = Engine::instance();
    auto* sceneMgr = engine ? engine->getSceneManager() : nullptr;

    for (auto& targetName : mTargetNodeNames) {
        auto* sceneObj = findTargetSceneObj(targetName);
        if (!sceneObj || !sceneObj->getSceneNode()) continue;
        auto it = mClones.find(targetName);
        if (it == mClones.end()) continue;

        if (!sceneObj->isEnabled()) {
            setCloneVisible(targetName, false);
            continue;
        }

        // Respect the SceneObjectNode's intended visibility (ParamSpec BOOL / DAG port).
        // The clone replaces the original, so the original SceneNode is always hidden.
        setCloneVisible(targetName, sceneObj->isNodeVisible());
        sceneObj->getSceneNode()->setVisible(false);

        // Position clone at target
        if (sceneMgr && it->second.entityCreated) {
            try {
                if (sceneMgr->hasSceneNode(it->second.sceneNodeName)) {
                    auto* fxSn = sceneMgr->getSceneNode(it->second.sceneNodeName);
                    fxSn->setPosition(sceneObj->getSceneNode()->_getDerivedPosition());
                    fxSn->setScale(sceneObj->getSceneNode()->_getDerivedScale());
                    fxSn->setOrientation(sceneObj->getSceneNode()->_getDerivedOrientation());
                }
            } catch (...) {}
        }
    }
}

void PerlinFxNode::update() {
    createDeferredEntities();
    resolveTargets();

    auto& inputs = getInputs();
    auto& outputs = getOutputs();

    float enableVal = inputs.at("enable")->getValue();
    if (enableVal >= 0.5f) {
        float disp = inputs.at("displacement")->getValue();
        float dens = inputs.at("density")->getValue();
        float tdens = inputs.at("timeDensity")->getValue();
        for (auto& [name, clone] : mClones) {
            clone.shader->setDisplacement(disp);
            clone.shader->setDensity(dens);
            clone.shader->setTimeDensity(tdens);
        }
        outputs.at("mesh_dirty")->setValue(1.0f);
    } else {
        outputs.at("mesh_dirty")->setValue(0.0f);
    }
    fireUpdate();
}

void PerlinFxNode::enable() {
    for (auto& [name, clone] : mClones) clone.shader->enable();
}

void PerlinFxNode::disable() {
    for (auto& [name, clone] : mClones) clone.shader->disable();
}

void PerlinFxNode::cleanup() {
    // Disable all shaders, restore target visibilities, and DESTROY clone OGRE objects
    auto* engine = Engine::instance();
    auto* sceneMgr = engine ? engine->getSceneManager() : nullptr;
    for (auto& [name, clone] : mClones) {
        clone.shader->disable();
        auto* sceneObj = findTargetSceneObj(name);
        if (sceneObj && sceneObj->isEnabled() && sceneObj->getSceneNode())
            sceneObj->getSceneNode()->setVisible(true);
        // Destroy clone OGRE objects (not just hide)
        if (sceneMgr && clone.entityCreated) {
            try {
                if (sceneMgr->hasSceneNode(clone.sceneNodeName)) {
                    auto* sn = sceneMgr->getSceneNode(clone.sceneNodeName);
                    sn->detachAllObjects();
                    sceneMgr->destroySceneNode(sn);
                }
                if (sceneMgr->hasEntity(clone.entityName)) {
                    sceneMgr->destroyEntity(clone.entityName);
                }
            } catch (...) {}
        }
    }
    mClones.clear();
}

void PerlinFxNode::onLinkChanged() {
    resolveTargets();
}

} // namespace bbfx
