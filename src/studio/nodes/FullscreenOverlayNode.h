#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <OgreSceneManager.h>
#include <OgreSceneNode.h>
#include <OgreBillboardSet.h>
#include <OgreRectangle2D.h>
#include <OgreMaterial.h>

namespace bbfx {

/// FullscreenOverlayNode — fullscreen quad reproducing the BBFx 2006 BillboardSet
/// pattern (cf. video.lua:49-80, textureset.lua:193-203).
///
/// Two modes:
///   - CameraLocked : Ogre::BillboardSet 1-billboard attached to the resolved
///                    camera's SceneNode, dimensioned to viewport w/h, positioned
///                    at Vector3(0, 0, -z_offset). Faithful 2006 reproduction.
///   - ScreenAligned: Ogre::Rectangle2D NDC fullscreen quad in RENDER_QUEUE_OVERLAY.
///                    Camera-independent.
///
/// Material is cloned per-instance for animatable alpha (setDiffuse/setSelfIllumination).
class FullscreenOverlayNode : public AnimationNode {
public:
    enum class Mode { CameraLocked, ScreenAligned };

    FullscreenOverlayNode(const std::string& name, Ogre::SceneManager* scene);
    ~FullscreenOverlayNode() override;

    void update() override;
    void cleanup() override;
    void setEnabled(bool en) override;
    void setUserVisible(bool v) override;
    void onLinkChanged() override;
    std::string getTypeName() const override { return "FullscreenOverlayNode"; }

    Mode getCurrentMode() const { return mCurrentMode; }
    Ogre::BillboardSet* getBillboardSet() const { return mBillboardSet; }
    Ogre::Rectangle2D*  getScreenQuad()   const { return mScreenQuad; }
    float getCurrentZOffset() const { return mCurrentZOffset; }
    const std::string& getCurrentMaterialName() const { return mCurrentMaterialName; }

private:
    Ogre::SceneManager* mScene = nullptr;
    Ogre::SceneNode*    mAttachNode = nullptr;       // for camera_locked mode
    bool                mOwnsAttachNode = false;     // true when attach node was created by this overlay (not the camera's node)
    Ogre::BillboardSet* mBillboardSet = nullptr;
    Ogre::Rectangle2D*  mScreenQuad = nullptr;       // for screen_aligned mode
    Ogre::SceneNode*    mScreenQuadNode = nullptr;   // for screen_aligned mode
    Ogre::MaterialPtr   mClonedMaterial;        // Holds the clone (mOwnsClone=true) OR an alias to a live producer material (mOwnsClone=false).
    std::string         mClonedMaterialName;
    bool                mOwnsClone = false;     // v3.5.2 Lot AU.25 — false when mClonedMaterial aliases a live upstream producer material (must NOT MaterialManager::remove() it).

    ParamSpec   mSpec;
    Mode        mCurrentMode = Mode::CameraLocked;
    std::string mCurrentMaterialName;
    float       mCurrentZOffset = 0.01f;
    float       mPrevAlpha = -1.0f;        // -1 forces first apply
    int         mLastViewportW = 0;        // I-1741: cached for poll-based resize tracking
    int         mLastViewportH = 0;

    static int  sCounter;

    Ogre::Camera* resolveCamera();
    void rebuildBillboard(Ogre::Camera* cam);
    void rebuildScreenQuad();
    void destroyBillboard();
    void destroyScreenQuad();
    void applyMaterial(const std::string& name, bool live);
    /// v3.5.2 Sprint S8 Lot AC — Pattern 3 consumer. `outLive` is set true when
    /// the resolved material comes from a runtime producer's `material_out`
    /// mirror (TextureBlend, VideoCrossfade, TheoraClip, …) — the caller must
    /// then bind it WITHOUT cloning so the producer's live TUS mutations
    /// (scrolls, texture name swaps) are visible on the overlay quad.
    std::string resolveMaterialFromSource(bool& outLive);
};

} // namespace bbfx
