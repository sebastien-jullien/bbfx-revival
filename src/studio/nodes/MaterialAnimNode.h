#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <OgreMaterial.h>
#include <OgreColourValue.h>
#include <OgreBlendMode.h>
#include <string>

namespace bbfx {

/// MaterialAnimNode — animates UV transforms (scroll U/V speed, rotate speed,
/// scale) and alpha of an existing OGRE material, via DAG ports.
///
/// Equivalent of the BBFx 2006 `TextureScrollControllerValue`,
/// `TextureRotateControllerValue` and `AlphaControllerValue` engine bricks.
///
/// The node backs up the original TUS state on the first apply and restores
/// it on cleanup / setEnabled(false).
class MaterialAnimNode : public AnimationNode {
public:
    explicit MaterialAnimNode(const std::string& name);
    ~MaterialAnimNode() override = default;

    void update() override;
    void cleanup() override;
    void setEnabled(bool en) override;
    std::string getTypeName() const override { return "MaterialAnimNode"; }

    const std::string& getTargetMaterial() const { return mTargetMaterial; }
    int  getLayerIndex() const { return mLayerIndex; }
    float getScrollUOffset() const { return mScrollUOffset; }
    float getScrollVOffset() const { return mScrollVOffset; }
    float getRotateOffset()  const { return mRotateOffset;  }

private:
    ParamSpec mSpec;
    std::string mTargetMaterial;
    int   mLayerIndex = 0;
    float mScrollUOffset = 0.0f;
    float mScrollVOffset = 0.0f;
    float mRotateOffset  = 0.0f;
    float mPrevDt = 1.0f / 60.0f;

    // Backup state
    bool  mBackupCaptured = false;
    Ogre::MaterialPtr mBackupMaterialRef;       // resolved material (cached)
    int   mBackupLayerIndex = -1;
    float mBackupTexUScroll = 0.0f, mBackupTexVScroll = 0.0f;
    float mBackupTexUScale  = 1.0f, mBackupTexVScale  = 1.0f;
    float mBackupTexRotateRad = 0.0f;
    Ogre::ColourValue mBackupDiffuse {1, 1, 1, 1};
    Ogre::SceneBlendType mBackupSceneBlending = Ogre::SBT_REPLACE;
    // N10 — `mBackupSceneBlendEnabled` retiré (membre mort : jamais lu/écrit).

    Ogre::MaterialPtr resolveMaterial();
    Ogre::TextureUnitState* resolveTUS(Ogre::MaterialPtr& outMat);
    void captureBackupIfNeeded();
    void restoreBackup();
};

} // namespace bbfx
