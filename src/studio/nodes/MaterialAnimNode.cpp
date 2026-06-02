#include "MaterialAnimNode.h"
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <cmath>
#include <iostream>

namespace bbfx {

MaterialAnimNode::MaterialAnimNode(const std::string& name)
    : AnimationNode(name)
{
    // ParamSpec
    ParamDef matDef;
    matDef.name = "target_material"; matDef.label = "Target Material";
    matDef.type = ParamType::MATERIAL;
    matDef.stringVal = "";
    mSpec.addParam(matDef);

    ParamDef layerDef;
    layerDef.name = "layer_index"; layerDef.label = "Layer (TUS) Index";
    layerDef.type = ParamType::INT;
    layerDef.intVal = 0; layerDef.minVal = 0; layerDef.maxVal = 7;
    mSpec.addParam(layerDef);

    setParamSpec(&mSpec);

    addInput(new AnimationPort("dt",              1.0f / 60.0f));
    addInput(new AnimationPort("scroll_u_speed",  0.0f));
    addInput(new AnimationPort("scroll_v_speed",  0.0f));
    addInput(new AnimationPort("rotate_speed",    0.0f));
    addInput(new AnimationPort("alpha",           1.0f));
    addInput(new AnimationPort("tex_scale_u",     1.0f));
    addInput(new AnimationPort("tex_scale_v",     1.0f));

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("scroll_u_speed", "Horizontal scroll speed in uv units / sec applied to the target TUS.");
    setPortTooltip("scroll_v_speed", "Vertical scroll speed in uv units / sec applied to the target TUS.");
    setPortTooltip("rotate_speed",   "Texture rotation speed (radians / sec).");
    setPortTooltip("alpha",          "Material global alpha 0..1 (multiplies the TUS alpha modifier).");
    setPortTooltip("tex_scale_u",    "Horizontal texture scale (1 = no zoom).");
    setPortTooltip("tex_scale_v",    "Vertical texture scale (1 = no zoom).");
}

Ogre::MaterialPtr MaterialAnimNode::resolveMaterial() {
    if (mTargetMaterial.empty()) return Ogre::MaterialPtr();
    return Ogre::MaterialManager::getSingleton().getByName(mTargetMaterial);
}

Ogre::TextureUnitState* MaterialAnimNode::resolveTUS(Ogre::MaterialPtr& outMat) {
    outMat = resolveMaterial();
    if (outMat.isNull()) return nullptr;
    if (outMat->getNumTechniques() == 0) return nullptr;
    auto* tech = outMat->getTechnique(0);
    if (tech->getNumPasses() == 0) return nullptr;
    auto* pass = tech->getPass(0);
    if (mLayerIndex < 0 || mLayerIndex >= (int)pass->getNumTextureUnitStates()) return nullptr;
    return pass->getTextureUnitState(mLayerIndex);
}

void MaterialAnimNode::captureBackupIfNeeded() {
    if (mBackupCaptured) return;
    Ogre::MaterialPtr mat;
    auto* tus = resolveTUS(mat);
    if (!tus || mat.isNull()) return;
    auto* pass = mat->getTechnique(0)->getPass(0);
    mBackupMaterialRef = mat;
    mBackupLayerIndex = mLayerIndex;
    mBackupTexUScroll = tus->getTextureUScroll();
    mBackupTexVScroll = tus->getTextureVScroll();
    mBackupTexUScale  = tus->getTextureUScale();
    mBackupTexVScale  = tus->getTextureVScale();
    mBackupTexRotateRad = tus->getTextureRotate().valueRadians();
    mBackupDiffuse = pass->getDiffuse();
    mBackupSceneBlending = pass->getSourceBlendFactor() == Ogre::SBF_SOURCE_ALPHA
                           ? Ogre::SBT_TRANSPARENT_ALPHA : Ogre::SBT_REPLACE;
    mBackupCaptured = true;
}

void MaterialAnimNode::restoreBackup() {
    if (!mBackupCaptured || mBackupMaterialRef.isNull()) return;
    if (mBackupLayerIndex < 0) return;
    if (mBackupMaterialRef->getNumTechniques() == 0) return;
    auto* pass = mBackupMaterialRef->getTechnique(0)->getPass(0);
    if (mBackupLayerIndex >= (int)pass->getNumTextureUnitStates()) return;
    auto* tus = pass->getTextureUnitState(mBackupLayerIndex);
    tus->setTextureScroll(mBackupTexUScroll, mBackupTexVScroll);
    tus->setTextureScale(mBackupTexUScale, mBackupTexVScale);
    tus->setTextureRotate(Ogre::Radian(mBackupTexRotateRad));
    pass->setDiffuse(mBackupDiffuse);
    pass->setSceneBlending(mBackupSceneBlending);
    mBackupCaptured = false;
}

void MaterialAnimNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (!en) restoreBackup();
}

void MaterialAnimNode::update() {
    if (!mEnabled) return;

    // Sync ParamSpec
    auto* matParam = mSpec.getParam("target_material");
    if (matParam && matParam->stringVal != mTargetMaterial) {
        // Material changed -> restore previous, capture new
        restoreBackup();
        mTargetMaterial = matParam->stringVal;
        mBackupMaterialRef.setNull();
        mBackupCaptured = false;
    }
    auto* layerParam = mSpec.getParam("layer_index");
    if (layerParam && layerParam->intVal != mLayerIndex) {
        restoreBackup();
        mLayerIndex = layerParam->intVal;
        mBackupCaptured = false;
    }

    Ogre::MaterialPtr mat;
    auto* tus = resolveTUS(mat);
    if (!tus || mat.isNull()) return;

    captureBackupIfNeeded();

    auto& in = getInputs();
    auto getf = [&in](const char* name, float def) {
        auto it = in.find(name);
        return (it != in.end()) ? it->second->getValue() : def;
    };

    float dt = getf("dt", 0.0f);
    if (dt <= 0.0f) dt = mPrevDt;
    else mPrevDt = dt;

    // Accumulators
    float sus = getf("scroll_u_speed", 0.0f);
    float svs = getf("scroll_v_speed", 0.0f);
    float rs  = getf("rotate_speed", 0.0f);

    mScrollUOffset = std::fmod(mScrollUOffset + sus * dt, 1.0f);
    mScrollVOffset = std::fmod(mScrollVOffset + svs * dt, 1.0f);
    mRotateOffset  = std::fmod(mRotateOffset  + rs  * dt, 6.2831853f);

    tus->setTextureScroll(mBackupTexUScroll + mScrollUOffset,
                          mBackupTexVScroll + mScrollVOffset);
    tus->setTextureRotate(Ogre::Radian(mBackupTexRotateRad + mRotateOffset));

    float scaleU = getf("tex_scale_u", 1.0f);
    float scaleV = getf("tex_scale_v", 1.0f);
    tus->setTextureScale(mBackupTexUScale * scaleU, mBackupTexVScale * scaleV);

    float alpha = getf("alpha", 1.0f);
    if (alpha < 0.0f) alpha = 0.0f; else if (alpha > 1.0f) alpha = 1.0f;
    auto* pass = mat->getTechnique(0)->getPass(0);
    auto diff = mBackupDiffuse;
    pass->setDiffuse(diff.r, diff.g, diff.b, alpha);
    if (alpha < 0.999f) {
        pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
    } else {
        pass->setSceneBlending(mBackupSceneBlending);
    }

    fireUpdate();
}

void MaterialAnimNode::cleanup() {
    restoreBackup();
}

} // namespace bbfx
