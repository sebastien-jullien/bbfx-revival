#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreBillboardSet.h>
#include <OgreMaterial.h>

namespace bbfx {

/// BillboardLayerNode — 3D companion to FullscreenOverlayNode. Reproduces the
/// BBFx 2006 `Object:fromBillboard(material, 720, 576)` pattern (cf. object.lua:90-108):
/// a single Billboard placed in the scene at a configurable position, with
/// either camera-facing (BBT_POINT) or perpendicular-common (BBT_PERPENDICULAR_COMMON)
/// orientation for oblique 3D layering.
class BillboardLayerNode : public AnimationNode {
public:
    BillboardLayerNode(const std::string& name, Ogre::SceneManager* scene);
    ~BillboardLayerNode() override;

    void update() override;
    void cleanup() override;
    void setEnabled(bool en) override;
    void setUserVisible(bool v) override;
    std::string getTypeName() const override { return "BillboardLayerNode"; }

    Ogre::SceneNode*    getSceneNode()    const { return mSceneNode; }
    Ogre::BillboardSet* getBillboardSet() const { return mBillboardSet; }
    bool                getFaceCamera()   const { return mFaceCamera; }

private:
    Ogre::SceneManager* mScene = nullptr;
    Ogre::SceneNode*    mSceneNode = nullptr;
    Ogre::BillboardSet* mBillboardSet = nullptr;
    Ogre::MaterialPtr   mClonedMaterial;
    std::string         mClonedMaterialName;
    bool                mOwnsClone = false;   // D13 — false = alias direct d'un producteur live

    ParamSpec mSpec;
    std::string mCurrentMaterialName;
    float mCurrentWidth  = 720.0f;
    float mCurrentHeight = 576.0f;
    bool  mFaceCamera = true;
    bool  mPrevFaceCamera = true;
    float mPrevAlpha = -1.0f;

    static int sCounter;

    void rebuildBillboard();
    void destroyBillboard();
    void applyMaterial(const std::string& name, bool live);
    std::string resolveMaterialFromSource(bool& outLive);  // v3.5.2 Sprint S8 Lot AC — Pattern 3 consumer
};

} // namespace bbfx
