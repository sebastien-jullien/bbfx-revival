#include "FullscreenOverlayNode.h"
#include "../../core/Animator.h"
#include "CameraNode.h"
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreRoot.h>
#include <OgreCamera.h>
#include <OgreViewport.h>
#include <OgreRenderTarget.h>
#include <OgreBillboard.h>
#include <cmath>
#include <iostream>

namespace bbfx {

int FullscreenOverlayNode::sCounter = 0;

FullscreenOverlayNode::FullscreenOverlayNode(const std::string& name, Ogre::SceneManager* scene)
    : AnimationNode(name), mScene(scene)
{
    // ParamSpec
    ParamDef matDef;
    matDef.name = "material"; matDef.label = "Material"; matDef.type = ParamType::MATERIAL;
    matDef.stringVal = "BaseWhite";
    mSpec.addParam(matDef);

    ParamDef modeDef;
    modeDef.name = "mode"; modeDef.label = "Mode"; modeDef.type = ParamType::ENUM;
    // D14 — `camera_locked` retiré des choix : ce mode (BillboardSet attaché à la
    // camera node) souffre d'une référence circulaire billboard↔camera et NE REND
    // PAS dans le viewport courant. Un overlay plein écran est par nature
    // screen-aligned (Rectangle2D NDC + RENDER_QUEUE_OVERLAY) — c'est le seul mode
    // fonctionnel et il couvre la viewport indépendamment de la camera. La parité
    // 2006 stricte (plein-écran ancré dans le scenegraph 3D) nécessiterait une
    // approche ManualObject/Plane mesh (roadmap v3.6) sans bénéfice visuel ici.
    modeDef.stringVal = "screen_aligned";
    modeDef.choices = {"screen_aligned"};
    mSpec.addParam(modeDef);

    ParamDef zDef;
    zDef.name = "z_offset"; zDef.label = "Z Offset"; zDef.type = ParamType::FLOAT;
    zDef.floatVal = 0.01f; zDef.minVal = 0.001f; zDef.maxVal = 1.0f; zDef.stepVal = 0.001f;
    mSpec.addParam(zDef);

    setParamSpec(&mSpec);

    // Ports DAG
    addInput(new AnimationPort("alpha", 1.0f));
    addInput(new AnimationPort("visible", 1.0f));
    addInput(new AnimationPort("camera_target", 0.0f));   // entity-link port
    addInput(new AnimationPort("z_offset", 0.01f));
    addInput(new AnimationPort("material_source", 0.0f, true));   // v3.5.2 Sprint S8 Lot AC — entity-link consumer (Pattern 3)

    // v3.5.2 Sprint S8 Lot AC — `entity` OUTPUT exposes this overlay as a target
    // for upstream MaterialBridgeNode (Pattern 1 producer side, identical to
    // SceneObjectNode). Enables `fso.entity → mb.entity` linking.
    addOutput(new AnimationPort("entity", 0.0f));

    // v3.5.2 Sprint S7 Lot Z — port tooltips (Inspector hover help)
    setPortTooltip("alpha",         "Animable opacity 0..1 of the fullscreen quad (multiplies material alpha).");
    setPortTooltip("visible",       "Boolean gate (>=0.5 visible). Use for beat-driven blink without rebuilding the quad.");
    setPortTooltip("camera_target", "Réservé (mode camera-locked indisponible dans cette version — overlay toujours screen-aligned). Conservé pour compat de projets.");
    setPortTooltip("z_offset",      "Réservé au mode camera-locked (indisponible). Sans effet sur le rendu screen-aligned.");
    setPortTooltip("material_source",
                   "Entity-link consumer (Pattern 3) — pulls a `material_out`/`texture_out` mirror from any upstream "
                   "producer (TextureBlend, VideoCrossfade, TheoraClip, NoiseTexture, Grayscale, ...). "
                   "When linked, supersedes the static `material` ParamSpec. Auto-wraps a texture name into "
                   "a single-TUS material if needed.");

    sCounter++;
}

FullscreenOverlayNode::~FullscreenOverlayNode() {
    cleanup();
}

void FullscreenOverlayNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (mBillboardSet) mBillboardSet->setVisible(en && mUserVisible);
    if (mScreenQuad)   mScreenQuad->setVisible(en && mUserVisible);
}

void FullscreenOverlayNode::setUserVisible(bool v) {
    AnimationNode::setUserVisible(v);
    if (mBillboardSet) mBillboardSet->setVisible(v && mEnabled);
    if (mScreenQuad)   mScreenQuad->setVisible(v && mEnabled);
}

void FullscreenOverlayNode::onLinkChanged() {
    // Reserved for I-1738 (camera_target re-resolution)
}

void FullscreenOverlayNode::update() {
    if (!mEnabled || !mScene) return;

    // D14 — un FullscreenOverlay rend toujours en ScreenAligned (Rectangle2D NDC +
    // RENDER_QUEUE_OVERLAY) : seul mode fonctionnel (cf. ctor). Tout ancien projet
    // sérialisé avec mode="camera_locked" retombe ici sans régression. On détruit un
    // éventuel BillboardSet résiduel (mode hérité) une seule fois.
    {
        Mode requested = Mode::ScreenAligned;
        if (requested != mCurrentMode) {
            if (mCurrentMode == Mode::CameraLocked) destroyBillboard();
            else                                    destroyScreenQuad();
        }
        mCurrentMode = requested;
    }

    // Sync z_offset (from port if connected, else from ParamSpec)
    auto& in = getInputs();
    auto itZ = in.find("z_offset");
    if (itZ != in.end()) {
        mCurrentZOffset = itZ->second->getValue();
    }

    // v3.5.2 Sprint S8 Lot AC — material resolution priority:
    //   1) `material_source` port linked → pull upstream mirror (Pattern 3)
    //   2) ParamSpec `material` STRING (default "BaseWhite")
    // The Pattern 3 consumer probes well-known producer mirror names and
    // auto-wraps a texture name into a single-TUS material if needed.
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

    // Apply alpha each frame via the clone — I-1740
    float alpha = 1.0f;
    auto itAlpha = in.find("alpha");
    if (itAlpha != in.end()) alpha = itAlpha->second->getValue();
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

    // Visible port (DAG): combine with mEnabled / mUserVisible — I-1740
    bool visiblePortOk = true;
    auto itVis = in.find("visible");
    if (itVis != in.end()) visiblePortOk = (itVis->second->getValue() > 0.5f);
    bool finalVisible = mEnabled && mUserVisible && visiblePortOk;
    if (mBillboardSet) mBillboardSet->setVisible(finalVisible);
    if (mScreenQuad)   mScreenQuad->setVisible(finalVisible);

    // Mode ScreenAligned: NDC fullscreen Rectangle2D (I-1739).
    if (mCurrentMode == Mode::ScreenAligned) {
        if (!mScreenQuad) rebuildScreenQuad();
        fireUpdate();
        return;
    }

    // Mode CameraLocked: create BillboardSet on demand and attach to camera SceneNode (I-1738).
    if (mCurrentMode == Mode::CameraLocked) {
        Ogre::Camera* cam = resolveCamera();
        if (!mBillboardSet) {
            rebuildBillboard(cam);
        }

        // Re-attach to camera's SceneNode if not already attached there.
        if (cam && mBillboardSet) {
            Ogre::SceneNode* camNode = cam->getParentSceneNode();
            if (camNode && camNode != mAttachNode) {
                if (mAttachNode) mAttachNode->detachObject(mBillboardSet);
                if (mOwnsAttachNode && mAttachNode) {
                    mScene->destroySceneNode(mAttachNode);
                }
                mAttachNode = camNode;
                mOwnsAttachNode = false;
                mAttachNode->attachObject(mBillboardSet);
            }

            // I-1741: poll viewport dimensions and track changes via cache.
            // v3.5.2 Sprint S8 Lot AK — convert pixel viewport size to WORLD-space
            // billboard dimensions using camera FOV + aspect ratio.
            auto* vp = cam->getViewport();
            if (vp) {
                int vw = vp->getActualWidth();
                int vh = vp->getActualHeight();
                if (vw > 0 && vh > 0 && (vw != mLastViewportW || vh != mLastViewportH)) {
                    Ogre::Radian fovY = cam->getFOVy();
                    Ogre::Real   ar   = static_cast<Ogre::Real>(vw) / static_cast<Ogre::Real>(vh);
                    Ogre::Real   nearPlane = cam->getNearClipDistance();
                    if (mCurrentZOffset < nearPlane * 1.05f) {
                        mCurrentZOffset = nearPlane * 1.05f;
                    }
                    float worldH = 2.0f * mCurrentZOffset * std::tan(fovY.valueRadians() * 0.5f);
                    float worldW = worldH * ar;
                    if (worldH <= 0.0f) worldH = 1.0f;
                    if (worldW <= 0.0f) worldW = 1.0f;
                    mBillboardSet->setDefaultDimensions(worldW, worldH);
                    mLastViewportW = vw;
                    mLastViewportH = vh;
                }
            }
        }

        // Reposition billboard if z_offset changed, and resync world-space dimensions
        // (depend on z_offset via the FOV formula).
        if (mBillboardSet && mBillboardSet->getNumBillboards() > 0) {
            auto* bb = mBillboardSet->getBillboard(0);
            const auto& p = bb->getPosition();
            if (p.z != -mCurrentZOffset) {
                bb->setPosition(Ogre::Vector3(0, 0, -mCurrentZOffset));
                if (cam) {
                    Ogre::Radian fovY = cam->getFOVy();
                    Ogre::Real   ar   = cam->getAspectRatio();
                    float worldH = 2.0f * mCurrentZOffset * std::tan(fovY.valueRadians() * 0.5f);
                    float worldW = worldH * ar;
                    if (worldH > 0 && worldW > 0) {
                        mBillboardSet->setDefaultDimensions(worldW, worldH);
                    }
                }
            }
        }
    }

    fireUpdate();
}

void FullscreenOverlayNode::cleanup() {
    destroyBillboard();
    destroyScreenQuad();

    if (!mClonedMaterial.isNull()) {
        // Only destroy clones we own. Live producer aliases (mOwnsClone=false)
        // belong to the upstream node (TextureBlend, TheoraClip, …) and must
        // outlive this overlay — removing them would yank the producer's
        // material from under it.
        if (mOwnsClone && !mClonedMaterialName.empty()) {
            Ogre::MaterialManager::getSingleton().remove(mClonedMaterialName);
        }
        mClonedMaterial.setNull();
        mClonedMaterialName.clear();
        mOwnsClone = false;
    }
}

Ogre::Camera* FullscreenOverlayNode::resolveCamera() {
    if (!mScene) return nullptr;

    // 1) Resolution via DAG entity-link on port "camera_target"
    auto* animator = Animator::instance();
    if (animator) {
        auto& in = getInputs();
        auto itCam = in.find("camera_target");
        if (itCam != in.end()) {
            auto sources = animator->getSourceNodes(itCam->second);
            for (auto* src : sources) {
                if (auto* camNode = dynamic_cast<CameraNode*>(src)) {
                    if (camNode->getCamera()) return camNode->getCamera();
                }
            }
        }
    }

    // 2) Fallback: main camera by name
    if (mScene->hasCamera("MainCamera")) {
        return mScene->getCamera("MainCamera");
    }

    // 3) Last fallback: first camera registered in the scene
    auto camIt = mScene->getMovableObjectIterator("Camera");
    if (camIt.hasMoreElements()) {
        return dynamic_cast<Ogre::Camera*>(camIt.getNext());
    }
    return nullptr;
}

void FullscreenOverlayNode::rebuildBillboard(Ogre::Camera* cam) {
    if (!mScene) return;
    destroyBillboard();

    const std::string uid = "fso_bb_" + std::to_string(sCounter) + "_" + getName();
    mBillboardSet = mScene->createBillboardSet(uid, 1);

    // v3.5.2 Sprint S8 Lot AK — Use BBT_PERPENDICULAR_COMMON with explicit
    // common direction = -Z (camera forward in lookAt scenenode space). This
    // FIXES the orientation degeneracy of BBT_POINT when the BillboardSet is
    // attached to the same SceneNode as the rendering camera (which created a
    // circular billboard-to-camera vector calculation, producing an undefined
    // orientation → billboard never rendered).
    mBillboardSet->setBillboardType(Ogre::BBT_PERPENDICULAR_COMMON);
    mBillboardSet->setCommonDirection(Ogre::Vector3::NEGATIVE_UNIT_Z); // forward in cam-node local space
    mBillboardSet->setCommonUpVector(Ogre::Vector3::UNIT_Y);

    // v3.5.2 Sprint S8 Lot AK — Frustum-fitting dimensions.
    // Pre-S8 bug : `setDefaultDimensions(viewportW_pixels, viewportH_pixels)` passed
    // PIXEL counts as WORLD units → quad colossal at near-plane distance, clipped invisible.
    // Correct geometry : at distance z from a perspective camera with FOVy,
    //   visible_height_world = 2 * z * tan(FOVy / 2)
    //   visible_width_world  = visible_height_world * aspectRatio
    // The billboard is placed at z = -z_offset, so |z| = z_offset.
    float w = 1.0f, h = 1.0f;
    if (cam) {
        Ogre::Radian fovY = cam->getFOVy();
        Ogre::Real   ar   = cam->getAspectRatio();
        // Ensure z_offset is in front of the near plane to avoid clipping.
        Ogre::Real   nearPlane = cam->getNearClipDistance();
        if (mCurrentZOffset < nearPlane * 1.05f) {
            mCurrentZOffset = nearPlane * 1.05f;
            if (auto* zp = mSpec.getParam("z_offset")) zp->floatVal = mCurrentZOffset;
        }
        // World-space frustum coverage at distance |z_offset|.
        h = 2.0f * mCurrentZOffset * std::tan(fovY.valueRadians() * 0.5f);
        w = h * ar;
        if (h <= 0.0f) h = 1.0f;
        if (w <= 0.0f) w = 1.0f;
    }
    mBillboardSet->setDefaultDimensions(w, h);

    mBillboardSet->createBillboard(Ogre::Vector3(0, 0, -mCurrentZOffset));

    // Apply current material via clone-per-instance (I-1740). The clone is
    // created the first time update() runs; here we attach the cloned name
    // if it already exists, else we set the bare name (clone happens later).
    if (!mClonedMaterial.isNull()) {
        // Use the material's actual name — works for clones (mClonedMaterialName)
        // AND for live aliases (Lot AU.25, where mClonedMaterialName is empty).
        mBillboardSet->setMaterialName(mClonedMaterial->getName());
    } else {
        auto* matParam = mSpec.getParam("material");
        const std::string& matName = (matParam && !matParam->stringVal.empty())
                                   ? matParam->stringVal : std::string("BaseWhite");
        mBillboardSet->setMaterialName(matName);
        mCurrentMaterialName = matName;
    }

    // I-1738 will attach to camera's SceneNode. For I-1737, attach to root so
    // the billboard exists in the scene graph (visibility validated by tests).
    mAttachNode = mScene->getRootSceneNode()->createChildSceneNode(uid + "_node");
    mAttachNode->attachObject(mBillboardSet);
    mOwnsAttachNode = true;

    mBillboardSet->setVisible(mEnabled && mUserVisible);
}

void FullscreenOverlayNode::rebuildScreenQuad() {
    if (!mScene) return;
    destroyScreenQuad();

    const std::string uid = "fso_quad_" + std::to_string(sCounter) + "_" + getName();

    mScreenQuad = new Ogre::Rectangle2D(true);
    mScreenQuad->setCorners(-1.0f, 1.0f, 1.0f, -1.0f); // NDC fullscreen
    mScreenQuad->setBoundingBox(Ogre::AxisAlignedBox::BOX_INFINITE);
    mScreenQuad->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY);

    if (!mClonedMaterial.isNull()) {
        mScreenQuad->setMaterial(mClonedMaterial);
    } else {
        auto* matParam = mSpec.getParam("material");
        const std::string& matName = (matParam && !matParam->stringVal.empty())
                                   ? matParam->stringVal : std::string("BaseWhite");
        auto matPtr = Ogre::MaterialManager::getSingleton().getByName(matName);
        if (!matPtr.isNull()) mScreenQuad->setMaterial(matPtr);
        mCurrentMaterialName = matName;
    }

    mScreenQuadNode = mScene->getRootSceneNode()->createChildSceneNode(uid + "_node");
    mScreenQuadNode->attachObject(mScreenQuad);

    mScreenQuad->setVisible(mEnabled && mUserVisible);
}

void FullscreenOverlayNode::destroyBillboard() {
    if (!mScene) return;
    if (mAttachNode && mBillboardSet) {
        mAttachNode->detachObject(mBillboardSet);
    }
    if (mBillboardSet) {
        mScene->destroyBillboardSet(mBillboardSet);
        mBillboardSet = nullptr;
    }
    if (mOwnsAttachNode && mAttachNode) {
        mScene->destroySceneNode(mAttachNode);
    }
    mAttachNode = nullptr;
    mOwnsAttachNode = false;
}

void FullscreenOverlayNode::destroyScreenQuad() {
    if (!mScene) return;
    if (mScreenQuadNode && mScreenQuad) {
        mScreenQuadNode->detachObject(mScreenQuad);
    }
    if (mScreenQuad) {
        delete mScreenQuad;
        mScreenQuad = nullptr;
    }
    if (mScreenQuadNode) {
        mScene->destroySceneNode(mScreenQuadNode);
        mScreenQuadNode = nullptr;
    }
}

std::string FullscreenOverlayNode::resolveMaterialFromSource(bool& outLive) {
    // v3.5.2 Sprint S8 Lot AC — Pattern 3 (entity-link consumer pulling a
    // ParamSpec mirror). Probe upstream nodes on the `material_source` port
    // for well-known producer mirrors. If the value resolves to a texture
    // name (not an existing material), auto-wrap into a single-TUS material.
    //
    // v3.5.2 Lot AU.25 — `outLive` distinguishes producers whose material
    // mutates per-frame (TUS texture name swaps, scrolls, …) — only the
    // `material_out` mirror is treated as live. The auto-wrap path stays
    // clone-able (the wrap material is static once created).
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
        // Live producers expose `material_out` (Pattern 2). Their material
        // mutates each frame and MUST be bound directly, not cloned.
        if (auto* p = spec->getParam("material_out"); p && !p->stringVal.empty()) {
            auto& mm = Ogre::MaterialManager::getSingleton();
            if (mm.getByName(p->stringVal).get()) {
                outLive = true;
                return p->stringVal;
            }
            // material_out points to a not-yet-created material → fall through to
            // the auto-wrap path below as a static fallback.
        }
        static const char* kMirrors[] = {
            "texture_out",      // Grayscale, NoiseTexture, Spectrogram
            "texture",          // legacy mirror
            "current_texture",  // TextureCycleNode
        };
        for (const char* mname : kMirrors) {
            auto* p = spec->getParam(mname);
            if (!p || p->stringVal.empty()) continue;
            const std::string& v = p->stringVal;
            // Material existant ? direct (clone-able — not a live mutator).
            auto& mm = Ogre::MaterialManager::getSingleton();
            if (mm.getByName(v).get()) return v;
            // Sinon, c'est un texture name — auto-wrap.
            std::string matName = "FullscreenOverlay/wrap/" + getName() + "/" + v;
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

void FullscreenOverlayNode::applyMaterial(const std::string& name, bool live) {
    if (name == mCurrentMaterialName && !mClonedMaterial.isNull()) return;

    // Drop previous clone if WE owned it. Live aliases must not be removed —
    // they belong to the upstream producer.
    if (!mClonedMaterial.isNull()) {
        if (mOwnsClone && !mClonedMaterialName.empty()) {
            Ogre::MaterialManager::getSingleton().remove(mClonedMaterialName);
        }
        mClonedMaterial.setNull();
        mClonedMaterialName.clear();
        mOwnsClone = false;
    }

    if (name.empty()) {
        mCurrentMaterialName = name;
        return;
    }

    auto base = Ogre::MaterialManager::getSingleton().getByName(name);
    if (base.isNull()) {
        mCurrentMaterialName = name; // accept reference even if not loaded yet
        return;
    }

    if (live) {
        // v3.5.2 Lot AU.25 — Producer-owned, mutating material (TextureBlend,
        // TheoraClip, VideoCrossfade, …). Bind the live pointer DIRECTLY so
        // per-frame TUS mutations (texture name swaps, scroll offsets, mask
        // sweeps) propagate to the overlay quad. Cloning here would snapshot
        // the producer's TUS state at first bind and freeze the visual — the
        // exact symptom of demo_texture_set "rien ne bouge".
        // The producer creates its material in the DEFAULT resource group so
        // textures resolve correctly (TextureBlendNode.cpp:127-142 contract).
        // Alpha / scene-blend setup below mutates the producer's material in
        // place — acceptable because an FX producer expects an overlay-style
        // consumer (single sink) and the alpha is the overlay's prerogative.
        mClonedMaterial = base;
        mClonedMaterialName.clear();        // no clone of our own
        mOwnsClone = false;
        mCurrentMaterialName = name;
    } else {
        mClonedMaterialName = "FullscreenOverlay/" + std::to_string(sCounter) + "/"
                            + getName() + "_mat";
        // Clone into the DEFAULT ("General") resource group so the clone's texture units can
        // resolve their texture files (the source material may live in OGRE's internal group,
        // which has no resource locations → textures wouldn't load → blank/invisible overlay).
        mClonedMaterial = base->clone(mClonedMaterialName, /*changeGroup=*/true,
                                      Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        mOwnsClone = true;
        mCurrentMaterialName = name;
    }

    // A fullscreen overlay is rendered in screen space — there is no lighting there.
    // Force every pass unlit so a *lit* source material shows its textures at full intensity
    // instead of going dark/black on the overlay quad.
    for (unsigned t = 0; t < mClonedMaterial->getNumTechniques(); ++t) {
        auto* tech = mClonedMaterial->getTechnique(t);
        for (unsigned p = 0; p < tech->getNumPasses(); ++p) {
            tech->getPass(p)->setLightingEnabled(false);
        }
    }

    // D14 — utiliser le nom réel du matériau résolu (mClonedMaterial->getName()) :
    // en branche `live`, mClonedMaterialName est vide → setMaterialName("") poserait
    // un matériau vide sur le billboard (bug latent, actif si camera_locked est un
    // jour réactivé). mScreenQuad utilise déjà le pointeur, donc OK.
    if (mBillboardSet && mClonedMaterial)
        mBillboardSet->setMaterialName(mClonedMaterial->getName());
    if (mScreenQuad)   mScreenQuad->setMaterial(mClonedMaterial);

    // A brand-new clone hasn't had the alpha/scene-blend setup applied yet — force the
    // alpha pass to re-run next update() (otherwise a clone that arrives while alpha is
    // unchanged stays fully opaque, with the source material's default scene_blend).
    mPrevAlpha = -1.0f;
}

// I-1741 note: OGRE 14 does NOT expose a "viewport dimensions changed" callback
// on RenderTargetListener (only viewportAdded / viewportRemoved / pre-post update).
// Polling viewport->getActualWidth() / getActualHeight() in update() is the
// canonical way to track resize. We do that here with mLastViewportW/H caches
// to avoid redundant setDefaultDimensions calls.

} // namespace bbfx
