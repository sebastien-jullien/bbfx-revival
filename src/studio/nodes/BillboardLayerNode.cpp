#include "BillboardLayerNode.h"
#include "../../core/Animator.h"
#include <OgreBillboard.h>
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgreTextureUnitState.h>
#include <OgrePass.h>
#include <iostream>

namespace bbfx {

int BillboardLayerNode::sCounter = 0;

BillboardLayerNode::BillboardLayerNode(const std::string& name, Ogre::SceneManager* scene)
    : AnimationNode(name), mScene(scene)
{
    sCounter++;

    ParamDef matDef;
    matDef.name = "material"; matDef.label = "Material"; matDef.type = ParamType::MATERIAL;
    matDef.stringVal = "BaseWhite";
    mSpec.addParam(matDef);

    ParamDef wDef;
    wDef.name = "width"; wDef.label = "Width"; wDef.type = ParamType::FLOAT;
    wDef.floatVal = 720.0f; wDef.minVal = 1.0f; wDef.maxVal = 4096.0f;
    mSpec.addParam(wDef);

    ParamDef hDef;
    hDef.name = "height"; hDef.label = "Height"; hDef.type = ParamType::FLOAT;
    hDef.floatVal = 576.0f; hDef.minVal = 1.0f; hDef.maxVal = 4096.0f;
    mSpec.addParam(hDef);

    ParamDef faceDef;
    faceDef.name = "face_camera"; faceDef.label = "Face Camera"; faceDef.type = ParamType::BOOL;
    faceDef.boolVal = true;
    mSpec.addParam(faceDef);

    setParamSpec(&mSpec);

    addInput(new AnimationPort("position.x", 0.0f));
    addInput(new AnimationPort("position.y", 0.0f));
    addInput(new AnimationPort("position.z", 0.0f));
    addInput(new AnimationPort("rotation.x", 0.0f));
    addInput(new AnimationPort("rotation.y", 0.0f));
    addInput(new AnimationPort("rotation.z", 0.0f));
    addInput(new AnimationPort("scale.x",    1.0f));
    addInput(new AnimationPort("scale.y",    1.0f));
    addInput(new AnimationPort("scale.z",    1.0f));
    addInput(new AnimationPort("alpha",      1.0f));
    addInput(new AnimationPort("visible",    1.0f));
    addInput(new AnimationPort("material_source", 0.0f, true));   // v3.5.2 Sprint S8 Lot AC — Pattern 3 consumer

    // v3.5.2 Sprint S8 Lot AC — `entity` OUTPUT exposes this billboard as
    // a target for upstream MaterialBridgeNode (Pattern 1 producer side).
    addOutput(new AnimationPort("entity", 0.0f));

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("position.x", "Billboard center X (world units). Animatable.");
    setPortTooltip("position.y", "Billboard center Y (world units). Animatable.");
    setPortTooltip("position.z", "Billboard center Z (world units). Animatable.");
    setPortTooltip("rotation.x", "Local X rotation (radians) — applied before face_camera.");
    setPortTooltip("rotation.y", "Local Y rotation (radians).");
    setPortTooltip("rotation.z", "Roll (radians) around the camera-facing axis.");
    setPortTooltip("scale.x",    "Width scale factor (1 = ParamSpec.width).");
    setPortTooltip("scale.y",    "Height scale factor (1 = ParamSpec.height).");
    setPortTooltip("scale.z",    "Depth scale (rarely used; only if face_camera off).");
    setPortTooltip("alpha",      "Per-billboard alpha override 0..1 (multiplies material alpha).");
    setPortTooltip("visible",    "Boolean gate (>=0.5 visible). Cheap blink without rebuilding.");
    setPortTooltip("material_source",
                   "Entity-link consumer (Pattern 3) — pulls a `material_out`/`texture_out` mirror from any upstream "
                   "producer. When linked, supersedes the static `material` ParamSpec. Auto-wraps texture names.");
}

BillboardLayerNode::~BillboardLayerNode() {
    cleanup();
}

void BillboardLayerNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (mBillboardSet) mBillboardSet->setVisible(en && mUserVisible);
}

void BillboardLayerNode::setUserVisible(bool v) {
    AnimationNode::setUserVisible(v);
    if (mBillboardSet) mBillboardSet->setVisible(v && mEnabled);
}

void BillboardLayerNode::rebuildBillboard() {
    if (!mScene) return;
    destroyBillboard();

    const std::string uid = "bbl_" + std::to_string(sCounter) + "_" + getName();
    mBillboardSet = mScene->createBillboardSet(uid, 1);
    mBillboardSet->setBillboardType(mFaceCamera ? Ogre::BBT_POINT
                                                 : Ogre::BBT_PERPENDICULAR_COMMON);
    if (!mFaceCamera) {
        mBillboardSet->setCommonDirection(Ogre::Vector3::UNIT_Z);
        mBillboardSet->setCommonUpVector(Ogre::Vector3::UNIT_Y);
    }
    mBillboardSet->setDefaultDimensions(mCurrentWidth, mCurrentHeight);
    mBillboardSet->createBillboard(Ogre::Vector3::ZERO);

    if (!mClonedMaterial.isNull()) {
        mBillboardSet->setMaterialName(mClonedMaterialName);
    } else {
        auto* matParam = mSpec.getParam("material");
        const std::string& name = (matParam && !matParam->stringVal.empty())
                                ? matParam->stringVal : std::string("BaseWhite");
        mBillboardSet->setMaterialName(name);
        mCurrentMaterialName = name;
    }

    mSceneNode = mScene->getRootSceneNode()->createChildSceneNode(uid + "_node");
    mSceneNode->attachObject(mBillboardSet);
    mBillboardSet->setVisible(mEnabled && mUserVisible);
}

void BillboardLayerNode::destroyBillboard() {
    if (!mScene) return;
    if (mSceneNode && mBillboardSet) {
        mSceneNode->detachObject(mBillboardSet);
    }
    if (mBillboardSet) {
        mScene->destroyBillboardSet(mBillboardSet);
        mBillboardSet = nullptr;
    }
    if (mSceneNode) {
        mScene->destroySceneNode(mSceneNode);
        mSceneNode = nullptr;
    }
}

std::string BillboardLayerNode::resolveMaterialFromSource(bool& outLive) {
    // v3.5.2 Sprint S8 Lot AC — Pattern 3 (entity-link consumer pulling
    // a ParamSpec mirror). Probe upstream nodes on `material_source` port
    // for well-known producer mirrors. Auto-wrap texture name → single-TUS
    // material if needed.
    // D13 — outLive=true quand la source est un producteur live (mirror
    // `material_out`, matériau existant) → applyMaterial liera le pointeur
    // directement au lieu de cloner (sinon le visuel se fige, cf. overlay Lot AU.25).
    outLive = false;
    auto* animator = Animator::instance();
    if (!animator) return std::string();

    auto& inputs = getInputs();
    auto it = inputs.find("material_source");
    if (it == inputs.end()) return std::string();

    auto sources = animator->getSourceNodes(it->second);
    for (auto* src : sources) {
        if (!src) continue;
        auto* spec = src->getParamSpec();
        if (!spec) continue;
        static const char* kMirrors[] = {
            "material_out", "texture_out", "texture", "current_texture",
        };
        for (const char* mname : kMirrors) {
            auto* p = spec->getParam(mname);
            if (!p || p->stringVal.empty()) continue;
            const std::string& v = p->stringVal;
            auto& mm = Ogre::MaterialManager::getSingleton();
            if (mm.getByName(v).get()) {
                // Un `material_out` existant = matériau produit/muté en live.
                outLive = (std::string(mname) == "material_out");
                return v;
            }
            std::string matName = "BillboardLayer/wrap/" + getName() + "/" + v;
            auto wrapped = mm.getByName(matName);
            if (!wrapped) {
                wrapped = mm.create(matName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
                auto* pass = wrapped->getTechnique(0)->getPass(0);
                pass->createTextureUnitState(v);
                pass->setLightingEnabled(false);
            }
            return matName;
        }
    }
    return std::string();
}

void BillboardLayerNode::applyMaterial(const std::string& name, bool live) {
    if (name == mCurrentMaterialName && !mClonedMaterial.isNull()) return;
    // Ne retirer que les clones que NOUS possédons (un alias live ne nous appartient pas).
    if (!mClonedMaterial.isNull() && mOwnsClone && !mClonedMaterialName.empty()) {
        Ogre::MaterialManager::getSingleton().remove(mClonedMaterialName);
    }
    mClonedMaterial.setNull();
    mClonedMaterialName.clear();
    mOwnsClone = false;
    if (name.empty()) {
        mCurrentMaterialName = name;
        return;
    }
    auto base = Ogre::MaterialManager::getSingleton().getByName(name);
    if (base.isNull()) {
        mCurrentMaterialName = name;
        return;
    }
    if (live) {
        // D13 — producteur live (material_out muté chaque frame) : on lie le
        // pointeur DIRECTEMENT pour que les mutations TUS propagent au billboard
        // (cloner figerait le visuel au premier bind — cf. FullscreenOverlay AU.25).
        mClonedMaterial = base;
        mOwnsClone = false;
    } else {
        mClonedMaterialName = "BillboardLayer/" + std::to_string(sCounter) + "/"
                            + getName() + "_mat";
        mClonedMaterial = base->clone(mClonedMaterialName);
        mOwnsClone = true;
    }
    mCurrentMaterialName = name;
    if (mBillboardSet) mBillboardSet->setMaterialName(mClonedMaterial->getName());
}

void BillboardLayerNode::update() {
    if (!mEnabled || !mScene) return;

    auto* faceParam = mSpec.getParam("face_camera");
    if (faceParam) mFaceCamera = faceParam->boolVal;
    auto* wParam = mSpec.getParam("width");
    if (wParam) mCurrentWidth = wParam->floatVal;
    auto* hParam = mSpec.getParam("height");
    if (hParam) mCurrentHeight = hParam->floatVal;

    bool needRebuild = (!mBillboardSet) || (mFaceCamera != mPrevFaceCamera);
    if (needRebuild) {
        rebuildBillboard();
        mPrevFaceCamera = mFaceCamera;
    } else if (mBillboardSet) {
        // Update dimensions in place
        mBillboardSet->setDefaultDimensions(mCurrentWidth, mCurrentHeight);
    }

    // v3.5.2 Sprint S8 Lot AC — material resolution priority :
    //   1) `material_source` Pattern 3 consumer (entity-link to upstream mirror)
    //   2) `material` ParamSpec STRING default "BaseWhite"
    bool requestedLive = false;
    std::string requestedMat = resolveMaterialFromSource(requestedLive);
    if (requestedMat.empty()) {
        auto* matParam = mSpec.getParam("material");
        requestedMat = (matParam && !matParam->stringVal.empty())
                     ? matParam->stringVal : std::string("BaseWhite");
        requestedLive = false;
    }
    if (requestedMat != mCurrentMaterialName || mClonedMaterial.isNull()) {
        applyMaterial(requestedMat, requestedLive);
    }

    auto& in = getInputs();
    if (mSceneNode) {
        mSceneNode->setPosition(in.at("position.x")->getValue(),
                                in.at("position.y")->getValue(),
                                in.at("position.z")->getValue());
        mSceneNode->setOrientation(Ogre::Quaternion::IDENTITY);
        mSceneNode->yaw  (Ogre::Radian(in.at("rotation.y")->getValue()));
        mSceneNode->pitch(Ogre::Radian(in.at("rotation.x")->getValue()));
        mSceneNode->roll (Ogre::Radian(in.at("rotation.z")->getValue()));
        mSceneNode->setScale(in.at("scale.x")->getValue(),
                             in.at("scale.y")->getValue(),
                             in.at("scale.z")->getValue());
    }

    float alpha = in.at("alpha")->getValue();
    if (alpha < 0.0f) alpha = 0.0f; else if (alpha > 1.0f) alpha = 1.0f;
    if (!mClonedMaterial.isNull() && alpha != mPrevAlpha) {
        for (unsigned t = 0; t < mClonedMaterial->getNumTechniques(); ++t) {
            auto* tech = mClonedMaterial->getTechnique(t);
            for (unsigned p = 0; p < tech->getNumPasses(); ++p) {
                auto* pass = tech->getPass(p);
                pass->setSelfIllumination(alpha, alpha, alpha);
                auto diff = pass->getDiffuse();
                pass->setDiffuse(diff.r, diff.g, diff.b, alpha);
                if (alpha < 0.999f) {
                    pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
                    pass->setDepthWriteEnabled(false);
                }
            }
        }
        mPrevAlpha = alpha;
    }

    bool visiblePort = (in.at("visible")->getValue() > 0.5f);
    if (mBillboardSet) mBillboardSet->setVisible(mEnabled && mUserVisible && visiblePort);

    fireUpdate();
}

void BillboardLayerNode::cleanup() {
    destroyBillboard();
    // D13 — ne retirer que les clones possédés (un alias live appartient au producteur).
    if (!mClonedMaterial.isNull() && mOwnsClone && !mClonedMaterialName.empty()) {
        Ogre::MaterialManager::getSingleton().remove(mClonedMaterialName);
    }
    mClonedMaterial.setNull();
    mClonedMaterialName.clear();
    mOwnsClone = false;
}

} // namespace bbfx
