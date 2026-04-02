#include "ViewportPicker.h"
#include "../../core/Animator.h"
#include "../../core/AnimationNode.h"
#include "../nodes/SceneObjectNode.h"
#include "../nodes/LightNode.h"
#include "../nodes/ParticleNode.h"
#include <OgreEntity.h>
#include <OgreMesh.h>
#include <OgreSubEntity.h>
#include <OgreSceneNode.h>
#include <OgreRay.h>
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreHighLevelGpuProgramManager.h>

namespace bbfx {

ViewportPicker::ViewportPicker(Ogre::SceneManager* sm, Ogre::Camera* cam)
    : mScene(sm), mCamera(cam)
{
    mRayQuery = mScene->createRayQuery(Ogre::Ray(), SCENE_QUERY_MASK);
    mRayQuery->setSortByDistance(true);
}

ViewportPicker::~ViewportPicker()
{
    if (mScene && mRayQuery) {
        mScene->destroyQuery(mRayQuery);
        mRayQuery = nullptr;
    }
}

Ogre::MovableObject* ViewportPicker::pick(float nx, float ny)
{
    if (!mCamera || !mRayQuery) return nullptr;

    Ogre::Ray ray = mCamera->getCameraToViewportRay(nx, ny);
    mRayQuery->setRay(ray);
    mRayQuery->setQueryMask(SCENE_QUERY_MASK);

    auto& results = mRayQuery->execute();
    for (auto& entry : results) {
        if (entry.movable && entry.distance > 0) {
            return entry.movable;
        }
    }
    return nullptr;
}

void ViewportPicker::select(Ogre::MovableObject* obj)
{
    if (mSelected != obj) {
        removeHighlight();
    }
    mSelected = obj;
    mSelectedDAGNode = obj ? findDAGNodeForEntity(obj) : "";
    applyHighlight();

    if (mSelectionCallback) {
        mSelectionCallback(mSelectedDAGNode);
    }
}

void ViewportPicker::selectByDAGName(const std::string& dagName)
{
    auto* animator = Animator::instance();
    if (!animator || dagName.empty()) {
        removeHighlight();
        mSelected = nullptr;
        mSelectedDAGNode = "";
        return;
    }

    auto* node = animator->getRegisteredNode(dagName);
    if (!node) {
        removeHighlight();
        mSelected = nullptr;
        mSelectedDAGNode = "";
        return;
    }

    // Remove highlight from previously selected object
    removeHighlight();

    mSelectedDAGNode = dagName;

    // Try to find the OGRE MovableObject for this node
    if (auto* soNode = dynamic_cast<SceneObjectNode*>(node)) {
        if (soNode->getSceneNode() && soNode->getSceneNode()->numAttachedObjects() > 0)
            mSelected = soNode->getSceneNode()->getAttachedObject(0);
        else
            mSelected = nullptr;
    } else {
        mSelected = nullptr;
    }

    // Apply highlight to the new selection
    applyHighlight();
    // Note: we do NOT fire mSelectionCallback here to avoid loops
}

void ViewportPicker::deselect()
{
    removeHighlight();
    mSelected = nullptr;
    mSelectedDAGNode = "";

    if (mSelectionCallback) {
        mSelectionCallback("");
    }
}

std::string ViewportPicker::findDAGNodeForEntity(Ogre::MovableObject* obj) const
{
    if (!obj) return "";
    auto* animator = Animator::instance();
    if (!animator) return "";

    auto names = animator->getRegisteredNodeNames();
    for (auto& name : names) {
        auto* node = animator->getRegisteredNode(name);
        if (!node) continue;

        // Check SceneObjectNode
        if (auto* soNode = dynamic_cast<SceneObjectNode*>(node)) {
            auto* sn = soNode->getSceneNode();
            if (sn) {
                for (unsigned i = 0; i < sn->numAttachedObjects(); ++i) {
                    if (sn->getAttachedObject(i) == obj) return name;
                }
            }
        }

        // Check LightNode - it has getSceneNode() too
        if (auto* lightNode = dynamic_cast<LightNode*>(node)) {
            auto* sn = lightNode->getSceneNode();
            if (sn) {
                for (unsigned i = 0; i < sn->numAttachedObjects(); ++i) {
                    if (sn->getAttachedObject(i) == obj) return name;
                }
            }
        }

        // Check ParticleNode
        if (auto* partNode = dynamic_cast<ParticleNode*>(node)) {
            auto* sn = partNode->getSceneNode();
            if (sn) {
                for (unsigned i = 0; i < sn->numAttachedObjects(); ++i) {
                    if (sn->getAttachedObject(i) == obj) return name;
                }
            }
        }
    }

    return "";
}

void ViewportPicker::applyHighlight()
{
    // Clean up any existing highlight first
    removeHighlight();

    if (!mSelected) return;
    auto* entity = dynamic_cast<Ogre::Entity*>(mSelected);
    if (!entity || !entity->getParentSceneNode()) return;

    // Ensure the wireframe material + shaders exist
    auto& matMgr = Ogre::MaterialManager::getSingleton();
    if (!matMgr.resourceExists("bbfx/selection_highlight", Ogre::RGN_DEFAULT)) {
        // Create GPU programs with inline GLSL source
        auto& gpuMgr = Ogre::HighLevelGpuProgramManager::getSingleton();
        if (!gpuMgr.resourceExists("bbfx/sel_highlight_vs", Ogre::RGN_DEFAULT)) {
            auto vp = gpuMgr.createProgram("bbfx/sel_highlight_vs", Ogre::RGN_DEFAULT,
                "glsl", Ogre::GPT_VERTEX_PROGRAM);
            vp->setSource(
                "#version 330 core\n"
                "in vec4 vertex;\n"
                "uniform mat4 worldViewProj;\n"
                "void main() { gl_Position = worldViewProj * vertex; }\n");
            vp->load();
        }
        if (!gpuMgr.resourceExists("bbfx/sel_highlight_fs", Ogre::RGN_DEFAULT)) {
            auto fp = gpuMgr.createProgram("bbfx/sel_highlight_fs", Ogre::RGN_DEFAULT,
                "glsl", Ogre::GPT_FRAGMENT_PROGRAM);
            fp->setSource(
                "#version 330 core\n"
                "out vec4 fragColour;\n"
                "void main() { fragColour = vec4(1.0, 0.5, 0.0, 1.0); }\n");
            fp->load();
        }

        auto mat = matMgr.create("bbfx/selection_highlight", Ogre::RGN_DEFAULT);
        auto* pass = mat->getTechnique(0)->getPass(0);
        pass->setPolygonMode(Ogre::PM_WIREFRAME);
        pass->setVertexProgram("bbfx/sel_highlight_vs");
        pass->getVertexProgramParameters()->setNamedAutoConstant(
            "worldViewProj", Ogre::GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX);
        pass->setFragmentProgram("bbfx/sel_highlight_fs");
        pass->setDepthCheckEnabled(true);
        pass->setDepthWriteEnabled(false);
        pass->setCullingMode(Ogre::CULL_NONE);
        pass->setDepthBias(1.0f);
    }

    // Create a clone entity for the wireframe overlay
    auto meshPtr = entity->getMesh();
    // Use unique name to avoid conflicts if OGRE caches entity names
    static int sOverlayCounter = 0;
    std::string overlayName = "bbfx_sel_overlay_" + std::to_string(++sOverlayCounter);
    mHighlightEntity = mScene->createEntity(overlayName, meshPtr->getName());
    mHighlightEntity->setQueryFlags(0);  // not pickable
    mHighlightEntity->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
    for (unsigned i = 0; i < mHighlightEntity->getNumSubEntities(); ++i) {
        mHighlightEntity->getSubEntity(i)->setMaterialName("bbfx/selection_highlight");
    }
    entity->getParentSceneNode()->attachObject(mHighlightEntity);
}

void ViewportPicker::removeHighlight()
{
    if (mHighlightEntity) {
        auto* parent = mHighlightEntity->getParentSceneNode();
        if (parent) parent->detachObject(mHighlightEntity);
        mScene->destroyEntity(mHighlightEntity);
        mHighlightEntity = nullptr;
    }
}

} // namespace bbfx
