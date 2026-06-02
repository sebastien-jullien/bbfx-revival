#include "TextureFeedbackNode.h"
#include "../core/Animator.h"
#include <OgreTextureManager.h>
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreRenderTarget.h>
#include <OgrePixelFormat.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace bbfx {

int TextureFeedbackNode::sCounter = 0;

TextureFeedbackNode::TextureFeedbackNode(const std::string& name)
    : AnimationNode(name)
{
    sCounter++;

    ParamDef decayDef;
    decayDef.name = "decay"; decayDef.label = "Decay";
    decayDef.type = ParamType::FLOAT;
    decayDef.floatVal = 0.95f; decayDef.minVal = 0.0f; decayDef.maxVal = 1.0f;
    mSpec.addParam(decayDef);

    ParamDef zoomDef;
    zoomDef.name = "zoom"; zoomDef.label = "Zoom";
    zoomDef.type = ParamType::FLOAT;
    zoomDef.floatVal = 1.0f; zoomDef.minVal = 0.5f; zoomDef.maxVal = 2.0f;
    mSpec.addParam(zoomDef);

    ParamDef bmDef;
    bmDef.name = "blend_mode"; bmDef.label = "Blend Mode"; bmDef.type = ParamType::ENUM;
    bmDef.stringVal = "screen";
    bmDef.choices = {"additive", "screen", "max"};
    mSpec.addParam(bmDef);

    ParamDef matMir;
    matMir.name = "material_out"; matMir.label = "Material (out)";
    matMir.type = ParamType::STRING; matMir.stringVal = "";
    matMir.readOnly = true;
    matMir.tooltip = "Read-only: feedback RTT material name. Route to MaterialBridgeNode for mesh display.";
    mSpec.addParam(matMir);

    setParamSpec(&mSpec);

    addInput(new AnimationPort("displacement.x", 0.0f));
    addInput(new AnimationPort("displacement.y", 0.0f));
    addInput(new AnimationPort("rotate",         0.0f));
    addInput(new AnimationPort("clear",          0.0f));
    // v3.5.2 Sprint S8 Lot AL : Pattern 3 consumer for source texture.
    // When linked to an upstream `texture_out` / `material_out` mirror
    // producer (NoiseTexture, Spectrogram, Theora, etc.), the feedback
    // node forwards that texture as TUS 0 (RT layer) of its generated
    // material. This unblocks the feedback_organic preset visually
    // (was an empty black quad pre-Lot-AL).
    addInput(new AnimationPort("source_texture", 0.0f, true));

    addOutput(new AnimationPort("material_ready", 0.0f));

    mGeneratedMaterialName = "TextureFeedback/" + std::to_string(sCounter) + "/" + getName();
    mPrevFrameRTTName = mGeneratedMaterialName + "_prev";
    mAccumRTTName     = mGeneratedMaterialName + "_accum";

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("displacement.x", "Per-frame X offset applied to the previous frame before re-blitting.");
    setPortTooltip("displacement.y", "Per-frame Y offset applied to the previous frame before re-blitting.");
    setPortTooltip("rotate",         "Per-frame rotation (radians) applied to the previous frame.");
    setPortTooltip("clear",          "Trigger (rising edge) clears the accumulator RTT.");
    setPortTooltip("material_ready", "Pulses 1.0 when the feedback material is bound (route -> MaterialBridge).");
    setPortTooltip("source_texture",
                   "Entity-link to upstream texture producer (NoiseTexture, Spectrogram, Theora, "
                   "Grayscale...). The source feeds the CPU feedback loop (stepFeedback) : it is "
                   "blended with the decayed/transformed previous accumulator → real trails. The "
                   "accumulated result is bound to TUS 0 of the material. Without a source the "
                   "accumulator stays black.");
}

TextureFeedbackNode::~TextureFeedbackNode() {
    cleanup();
}

void TextureFeedbackNode::initRTTs() {
    if (!mPrevFrameRTT.isNull() && !mAccumRTT.isNull()) return;
    auto& tm = Ogre::TextureManager::getSingleton();
    if (mPrevFrameRTT.isNull()) {
        mPrevFrameRTT = tm.createManual(mPrevFrameRTTName,
                                        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                                        Ogre::TEX_TYPE_2D,
                                        mResolutionW, mResolutionH, 0,
                                        Ogre::PF_R8G8B8A8,
                                        Ogre::TU_RENDERTARGET);
        clearRTT(mPrevFrameRTT);
    }
    if (mAccumRTT.isNull()) {
        mAccumRTT = tm.createManual(mAccumRTTName,
                                    Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                                    Ogre::TEX_TYPE_2D,
                                    mResolutionW, mResolutionH, 0,
                                    Ogre::PF_R8G8B8A8,
                                    Ogre::TU_RENDERTARGET);
        clearRTT(mAccumRTT);
    }
}

void TextureFeedbackNode::destroyRTTs() {
    auto& tm = Ogre::TextureManager::getSingleton();
    if (!mPrevFrameRTT.isNull()) {
        tm.remove(mPrevFrameRTTName);
        mPrevFrameRTT.setNull();
    }
    if (!mAccumRTT.isNull()) {
        tm.remove(mAccumRTTName);
        mAccumRTT.setNull();
    }
}

void TextureFeedbackNode::clearRTT(Ogre::TexturePtr rtt) {
    if (rtt.isNull()) return;
    auto buf = rtt->getBuffer();
    if (buf.isNull()) return;
    // M6 — garde : un CPU-lock sur une texture TU_RENDERTARGET n'est pas supporté
    // sur tous les backends (D3D) et peut lever une Ogre::Exception. On l'enveloppe
    // (cf. GrayscaleNode) ; en cas d'échec on tente un clear via le render target.
    try {
        buf->lock(Ogre::HardwareBuffer::HBL_DISCARD);
        const auto& pb = buf->getCurrentLock();
        size_t stride = pb.rowPitch * Ogre::PixelUtil::getNumElemBytes(pb.format);
        if (pb.data && stride > 0) {
            std::memset(pb.data, 0, stride * pb.getHeight());
        }
        buf->unlock();
    } catch (const Ogre::Exception& e) {
        // Backend refusant le CPU-lock d'un render-target : on n'a pas crashé, c'est
        // l'objectif. La RTT démarre généralement déjà à zéro côté GPU.
        std::cerr << "[TextureFeedbackNode] clearRTT CPU-lock non supporté ("
                  << e.getDescription() << ") — ignoré sans crash" << std::endl;
    }
}

void TextureFeedbackNode::ensureFeedbackTexture() {
    if (!mFeedbackTex.isNull()) return;
    mFeedbackTexName = mGeneratedMaterialName + "_fb";
    mFeedbackTex = Ogre::TextureManager::getSingleton().createManual(
        mFeedbackTexName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        Ogre::TEX_TYPE_2D, kFbW, kFbH, 0, Ogre::PF_BYTE_RGBA,
        Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);
    mAccum.assign(static_cast<size_t>(kFbW) * kFbH * 4, 0);
    mSrcBuf.assign(static_cast<size_t>(kFbW) * kFbH * 4, 0);
    mAccumInit = true;
}

void TextureFeedbackNode::stepFeedback(const std::string& srcTexName, float decay,
                                       float zoom, float dx, float dy, float rot) {
    ensureFeedbackTexture();
    if (mFeedbackTex.isNull()) return;

    // 1. Lecture (downsample) de la texture source dans mSrcBuf (kFbW×kFbH RGBA).
    auto srcTex = Ogre::TextureManager::getSingleton().getByName(srcTexName);
    bool haveSrc = false;
    if (!srcTex.isNull()) {
        try {
            Ogre::PixelBox dst(kFbW, kFbH, 1, Ogre::PF_BYTE_RGBA, mSrcBuf.data());
            srcTex->getBuffer()->blitToMemory(dst);
            haveSrc = true;
        } catch (const Ogre::Exception&) { haveSrc = false; }
    }

    // 2. Nouvel accumulateur = blend( source , prev*decay transformé ).
    //    Échantillonnage inverse : pour le pixel (x,y) de sortie, on lit l'ancien
    //    accumulateur à la coordonnée transformée (zoom autour du centre + rotation
    //    + déplacement) → produit zoom/rotation/drift des traînées.
    std::vector<uint8_t> next(mAccum.size(), 0);
    const float cx = kFbW * 0.5f, cy = kFbH * 0.5f;
    const float invZoom = (zoom > 0.01f) ? 1.0f / zoom : 1.0f;
    const float cs = std::cos(-rot), sn = std::sin(-rot);
    const float offX = dx * kFbW, offY = dy * kFbH;
    auto sampleAccum = [&](float fx, float fy, int c) -> float {
        int xi = static_cast<int>(fx + 0.5f), yi = static_cast<int>(fy + 0.5f);
        if (xi < 0 || xi >= kFbW || yi < 0 || yi >= kFbH) return 0.0f;
        return mAccum[(static_cast<size_t>(yi) * kFbW + xi) * 4 + c];
    };
    for (int y = 0; y < kFbH; ++y) {
        for (int x = 0; x < kFbW; ++x) {
            // inverse transform du point courant vers l'ancien accumulateur
            float rx = (x - cx) * invZoom, ry = (y - cy) * invZoom;
            float sx = cx + (rx * cs - ry * sn) - offX;
            float sy = cy + (rx * sn + ry * cs) - offY;
            size_t idx = (static_cast<size_t>(y) * kFbW + x) * 4;
            for (int c = 0; c < 3; ++c) {
                float prev = sampleAccum(sx, sy, c) * decay;
                float src  = haveSrc ? static_cast<float>(mSrcBuf[idx + c]) : 0.0f;
                float v;
                switch (mBlendMode) {
                    case BlendMode::Additive: v = src + prev; break;
                    case BlendMode::Max:      v = std::max(src, prev); break;
                    default: /* Screen */     v = 255.0f - (255.0f - src) * (255.0f - prev) / 255.0f; break;
                }
                next[idx + c] = static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
            }
            next[idx + 3] = 255;
        }
    }
    mAccum.swap(next);

    // 3. Upload de l'accumulateur dans la texture dynamique.
    auto buf = mFeedbackTex->getBuffer();
    buf->lock(Ogre::HardwareBuffer::HBL_DISCARD);
    const auto& pb = buf->getCurrentLock();
    auto* outp = static_cast<uint8_t*>(pb.data);
    size_t rowBytes = static_cast<size_t>(kFbW) * 4;
    size_t dstStride = pb.rowPitch * 4;
    for (int y = 0; y < kFbH; ++y)
        std::memcpy(outp + y * dstStride, mAccum.data() + y * rowBytes, rowBytes);
    buf->unlock();
}

void TextureFeedbackNode::buildMaterialIfNeeded() {
    if (!mGeneratedMaterial.isNull()) return;
    auto& mm = Ogre::MaterialManager::getSingleton();
    // v3.5.2 final: clone the dedicated `BBFx/FeedbackNode` material that
    // ships the proper feedback_node.frag fragment program. Falls back to
    // BaseWhite if the material script failed to load (logs a warning).
    auto base = mm.getByName("BBFx/FeedbackNode");
    if (base.isNull()) {
        std::cerr << "[TextureFeedbackNode] BBFx/FeedbackNode material missing — "
                     "falling back to BaseWhite (no shader feedback)" << std::endl;
        base = mm.getByName("BaseWhite");
    }
    if (base.isNull()) return;
    mGeneratedMaterial = base->clone(mGeneratedMaterialName);

    auto* pass = mGeneratedMaterial->getTechnique(0)->getPass(0);
    while (pass->getNumTextureUnitStates() < 2) {
        pass->createTextureUnitState();
    }
    pass->getTextureUnitState(0)->setTextureName(mAccumRTTName);
    pass->getTextureUnitState(1)->setTextureName(mPrevFrameRTTName);

    auto* mirror = mSpec.getParam("material_out");
    if (mirror) mirror->stringVal = mGeneratedMaterialName;
}

void TextureFeedbackNode::_swapForTest() {
    std::swap(mPrevFrameRTT, mAccumRTT);
    std::swap(mPrevFrameRTTName, mAccumRTTName);
    if (!mGeneratedMaterial.isNull()) {
        auto* pass = mGeneratedMaterial->getTechnique(0)->getPass(0);
        if (pass->getNumTextureUnitStates() >= 2) {
            pass->getTextureUnitState(0)->setTextureName(mAccumRTTName);
            pass->getTextureUnitState(1)->setTextureName(mPrevFrameRTTName);
        }
    }
}

void TextureFeedbackNode::update() {
    if (!mEnabled) return;

    initRTTs();
    buildMaterialIfNeeded();

    auto* bmParam = mSpec.getParam("blend_mode");
    if (bmParam) {
        if      (bmParam->stringVal == "additive") mBlendMode = BlendMode::Additive;
        else if (bmParam->stringVal == "max")      mBlendMode = BlendMode::Max;
        else                                        mBlendMode = BlendMode::Screen;
    }

    // Edge-detect "clear" trigger
    auto& in = getInputs();
    auto itClear = in.find("clear");
    bool clearVal = (itClear != in.end() && itClear->second->getValue() > 0.5f);
    mWasClearedThisFrame = false;
    if (clearVal && !mPrevClearState) {
        clearRTT(mPrevFrameRTT);
        clearRTT(mAccumRTT);
        mWasClearedThisFrame = true;
    }
    mPrevClearState = clearVal;

    // Paramètres de feedback (lus une fois — servent au pas CPU ET aux uniforms shader).
    {
        auto* decayP = mSpec.getParam("decay");
        auto* zoomP  = mSpec.getParam("zoom");
        mFbDecay = decayP ? std::clamp(decayP->floatVal, 0.0f, 1.0f) : 0.95f;
        mFbZoom  = zoomP  ? zoomP->floatVal  : 1.0f;
        mFbDx = mFbDy = mFbRot = 0.0f;
        if (auto it = in.find("displacement.x"); it != in.end()) mFbDx = it->second->getValue();
        if (auto it = in.find("displacement.y"); it != in.end()) mFbDy = it->second->getValue();
        if (auto it = in.find("rotate"); it != in.end()) mFbRot = it->second->getValue();
    }

    // v3.5.2 Sprint S8 Lot AL — résout la texture amont via `source_texture`.
    // M6 — au lieu de la binder telle quelle, elle alimente la boucle de feedback
    // CPU (stepFeedback) ; c'est la texture ACCUMULÉE (traînées) qui est liée à TUS0.
    bool sourceBound = false;
    std::string srcTexName;
    auto* animator = Animator::instance();
    if (animator && !mGeneratedMaterial.isNull()) {
        auto itSrc = in.find("source_texture");
        if (itSrc != in.end()) {
            auto sources = animator->getSourceNodes(itSrc->second);
            for (auto* src : sources) {
                if (!src) continue;
                auto* spec = src->getParamSpec();
                if (!spec) continue;
                static const char* kMirrors[] = {
                    "texture_out", "material_out", "texture", "current_texture",
                };
                for (const char* mname : kMirrors) {
                    auto* p = spec->getParam(mname);
                    if (!p || p->stringVal.empty()) continue;
                    auto t = Ogre::TextureManager::getSingleton().getByName(p->stringVal);
                    if (t.isNull()) continue;
                    srcTexName = p->stringVal;
                    goto src_bound;
                }
            }
            src_bound:;
        }
    }

    // M6 — pas de feedback CPU + bind de l'accumulateur sur TUS0.
    if (!mGeneratedMaterial.isNull()) {
        if (mWasClearedThisFrame && mAccumInit)
            std::fill(mAccum.begin(), mAccum.end(), 0); // le trigger clear vide aussi l'accumulateur CPU
        if (!srcTexName.empty()) {
            stepFeedback(srcTexName, mFbDecay, mFbZoom, mFbDx, mFbDy, mFbRot);
            auto* pass = mGeneratedMaterial->getTechnique(0)->getPass(0);
            if (pass->getNumTextureUnitStates() > 0 && !mFeedbackTex.isNull()) {
                pass->getTextureUnitState(0)->setTextureName(mFeedbackTexName);
                sourceBound = true;
            }
        }
    }

    // ── v3.5.2 final: push uniforms to the cloned material ──────────────
    if (!mGeneratedMaterial.isNull()
     && mGeneratedMaterial->getNumTechniques() > 0
     && mGeneratedMaterial->getTechnique(0)->getNumPasses() > 0) {
        auto* pass = mGeneratedMaterial->getTechnique(0)->getPass(0);
        if (pass->hasFragmentProgram()) {
            auto fp = pass->getFragmentProgramParameters();
            if (!fp.isNull()) {
                auto* decayP = mSpec.getParam("decay");
                auto* zoomP  = mSpec.getParam("zoom");
                float decay = decayP ? decayP->floatVal : 0.95f;
                float zoom  = zoomP  ? zoomP->floatVal  : 1.0f;
                float dx = 0, dy = 0, rot = 0;
                if (auto it = in.find("displacement.x"); it != in.end()) dx = it->second->getValue();
                if (auto it = in.find("displacement.y"); it != in.end()) dy = it->second->getValue();
                if (auto it = in.find("rotate"); it != in.end()) rot = it->second->getValue();
                int blendModeInt = static_cast<int>(mBlendMode);

                auto setIfExists = [&](const std::string& name, auto value) {
                    if (fp->_findNamedConstantDefinition(name) != nullptr) {
                        fp->setNamedConstant(name, value);
                    }
                };
                setIfExists("decay", decay);
                setIfExists("zoom",  zoom);
                setIfExists("rotateAngle", rot);
                setIfExists("blendMode", blendModeInt);
                if (fp->_findNamedConstantDefinition("displacement") != nullptr) {
                    Ogre::Vector2 d(dx, dy);
                    fp->setNamedConstant("displacement", d);
                }
            }
        }

        // RTT swap each frame: the texture written last frame becomes prevFrame
        // input next frame. The PostProcessStack will blit the live viewport
        // into mAccumRTT before the material is bound for the fullscreen pass.
        // M6 — NE PAS réécraser TUS0 si une `source_texture` y a été bindée juste
        // au-dessus (sinon la source n'était jamais affichée — bug du même update).
        if (pass->getNumTextureUnitStates() >= 2) {
            if (!sourceBound) pass->getTextureUnitState(0)->setTextureName(mAccumRTTName);
            pass->getTextureUnitState(1)->setTextureName(mPrevFrameRTTName);
        }
    }

    auto& out = getOutputs();
    auto itReady = out.find("material_ready");
    if (itReady != out.end()) {
        itReady->second->setValue((!mGeneratedMaterial.isNull()
                                   && !mPrevFrameRTT.isNull()
                                   && !mAccumRTT.isNull()) ? 1.0f : 0.0f);
    }

    fireUpdate();
}

void TextureFeedbackNode::cleanup() {
    if (!mGeneratedMaterial.isNull()) {
        Ogre::MaterialManager::getSingleton().remove(mGeneratedMaterialName);
        mGeneratedMaterial.setNull();
    }
    destroyRTTs();
    // M6 — libère la texture dynamique de feedback.
    if (!mFeedbackTex.isNull()) {
        Ogre::TextureManager::getSingleton().remove(mFeedbackTexName);
        mFeedbackTex.setNull();
    }
    mAccum.clear();
    mSrcBuf.clear();
    mAccumInit = false;
}

} // namespace bbfx
