#include "VideoCrossfadeNode.h"
#include "../../core/Animator.h"
#include "../../core/PrimitiveNodes.h"
#include "../../video/TheoraClipNode.h"
#include "../../video/TheoraClip.h"
#include <OgreMaterialManager.h>
#include <OgreRTShaderSystem.h>
#include <cmath>
#include <iostream>

namespace bbfx {

int VideoCrossfadeNode::sCounter = 0;

VideoCrossfadeNode::VideoCrossfadeNode(const std::string& name)
    : AnimationNode(name)
{
    sCounter++;

    // Read-only mirror for the generated material name.
    ParamDef matOutDef;
    matOutDef.name = "material_out"; matOutDef.label = "Material (out)";
    matOutDef.type = ParamType::STRING; matOutDef.stringVal = "";
    matOutDef.readOnly = true;
    matOutDef.tooltip = "Read-only: crossfaded video material name. Routable to MaterialBridgeNode.";
    mSpec.addParam(matOutDef);

    setParamSpec(&mSpec);

    addInput(new AnimationPort("dt",                 1.0f / 60.0f));
    addInput(new AnimationPort("clip_a",             0.0f));        // entity-link
    addInput(new AnimationPort("clip_b",             0.0f));        // entity-link
    addInput(new AnimationPort("beta",               0.0f));
    addInput(new AnimationPort("auto_crossfade_bpm", 0.0f));

    addOutput(new AnimationPort("material_ready", 0.0f));

    mGeneratedMaterialName = "video/crossfader/" + std::to_string(sCounter) + "/" + getName();

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("clip_a",             "Entity-link to TheoraClipNode A. Connect from clip A's 'entity' output port.");
    setPortTooltip("clip_b",             "Entity-link to TheoraClipNode B. Connect from clip B's 'entity' output port.");
    setPortTooltip("beta",               "Crossfade weight 0..1 (0 = full A, 1 = full B). Animatable for manual control.");
    setPortTooltip("auto_crossfade_bpm", "If > 0, oscillates beta automatically at this BPM. 0 = manual control.");
    setPortTooltip("material_ready",     "Niveau 1.0 tant que le matériau crossfadé est lié (route -> MaterialBridge -> mesh).");
}

AnimationNode* VideoCrossfadeNode::resolveClipNode(const std::string& portName) {
    auto* animator = Animator::instance();
    if (!animator) return nullptr;
    auto& in = getInputs();
    auto it = in.find(portName);
    if (it == in.end()) return nullptr;
    auto sources = animator->getSourceNodes(it->second);
    for (auto* src : sources) {
        if (auto* clipNode = dynamic_cast<TheoraClipNode*>(src)) {
            return clipNode;
        }
    }
    return nullptr;
}

void VideoCrossfadeNode::update() {
    if (!mEnabled) return;

    auto* nodeA = resolveClipNode("clip_a");
    auto* nodeB = resolveClipNode("clip_b");
    auto* clipA = nodeA ? dynamic_cast<TheoraClipNode*>(nodeA)->getClip() : nullptr;
    auto* clipB = nodeB ? dynamic_cast<TheoraClipNode*>(nodeB)->getClip() : nullptr;

    std::string texA = clipA ? clipA->getTextureName() : std::string();
    std::string texB = clipB ? clipB->getTextureName() : std::string();

    // Graceful fallback: if only one clip is resolved, build the crossfader
    // with that texture for both slots so the visible side is always the valid one.
    if (texA.empty() && !texB.empty()) texA = texB;
    if (texB.empty() && !texA.empty()) texB = texA;

    // (Re)build crossfader if textures changed (or first build).
    if ((texA != mPrevTexA || texB != mPrevTexB) && !texA.empty() && !texB.empty()) {
        mCrossfader.reset();  // destroys old material
        mCrossfader = std::make_unique<TextureCrossfader>(mGeneratedMaterialName, texA, texB);
        mPrevTexA = texA;
        mPrevTexB = texB;
        mPrevBeta = -1.0f; // force first crossfade() apply

        auto* mirror = mSpec.getParam("material_out");
        if (mirror) mirror->stringVal = mGeneratedMaterialName;
    }

    if (!mCrossfader) {
        auto& out = getOutputs();
        auto itReady = out.find("material_ready");
        if (itReady != out.end()) itReady->second->setValue(0.0f);
        return;
    }

    // Compute beta — auto BPM oscillation overrides the port if enabled.
    auto& in = getInputs();
    float beta = 0.0f;
    auto itBeta = in.find("beta");
    if (itBeta != in.end()) beta = itBeta->second->getValue();

    auto itAuto = in.find("auto_crossfade_bpm");
    float autoBpm = (itAuto != in.end()) ? itAuto->second->getValue() : 0.0f;
    if (autoBpm > 0.0f) {
        float dt = 0.0f;
        auto itDt = in.find("dt");
        if (itDt != in.end()) dt = itDt->second->getValue();
        if (dt <= 0.0f) dt = mPrevDt;
        else mPrevDt = dt;
        mAccumulatedTime += dt;

        auto* root = RootTimeNode::instance();
        float bpm = root ? root->getBPM() : 120.0f;
        if (bpm <= 0.0f) bpm = 120.0f;
        // period for one full sine cycle, scaled by autoBpm (1=quarter, 2=eighth, ...).
        float period = 60.0f * autoBpm / bpm;
        if (period <= 0.001f) period = 0.001f;
        beta = std::sin(mAccumulatedTime / period * 6.2831853f) * 0.5f + 0.5f;
    }

    if (beta < 0.0f) beta = 0.0f; else if (beta > 1.0f) beta = 1.0f;
    if (std::abs(beta - mPrevBeta) > 1e-5f) {
        mCrossfader->crossfade(beta);
        mPrevBeta = beta;

        // I-2055 (Lot AV.4 round 2) — même fix RTSS que TextureBlendNode (Lot AV.2) :
        // crossfade() modifie `LBX_BLEND_MANUAL` sur la TUS 1 via `setColourOperationEx`
        // avec `mBeta` comme constante de blend. RTSS a compilé un shader avec la valeur
        // initiale 0 baked → les beta changes ne sont jamais visibles à l'écran (la
        // technique alternative shader-based reste figée). On force RTSS à régénérer.
        auto matPtr = Ogre::MaterialManager::getSingleton().getByName(mGeneratedMaterialName);
        if (!matPtr.isNull()) {
            if (auto* sg = Ogre::RTShader::ShaderGenerator::getSingletonPtr()) {
                sg->invalidateMaterial(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME,
                                       mGeneratedMaterialName, matPtr->getGroup());
            }
        }
    }

    auto& out = getOutputs();
    auto itReady = out.find("material_ready");
    if (itReady != out.end()) itReady->second->setValue(1.0f);

    fireUpdate();
}

void VideoCrossfadeNode::_buildWithMockTextures(const std::string& texA,
                                                 const std::string& texB) {
    mCrossfader.reset();
    mCrossfader = std::make_unique<TextureCrossfader>(mGeneratedMaterialName, texA, texB);
    mPrevTexA = texA;
    mPrevTexB = texB;
    mPrevBeta = -1.0f;
    auto* mirror = mSpec.getParam("material_out");
    if (mirror) mirror->stringVal = mGeneratedMaterialName;
}

void VideoCrossfadeNode::cleanup() {
    mCrossfader.reset();
    // bbfx::TextureCrossfader's default destructor does not remove the material
    // it created via MaterialManager::create. We do it here to avoid a leak.
    if (!mGeneratedMaterialName.empty()) {
        auto& mm = Ogre::MaterialManager::getSingleton();
        if (!mm.getByName(mGeneratedMaterialName).isNull()) {
            mm.remove(mGeneratedMaterialName);
        }
    }
    mPrevTexA.clear();
    mPrevTexB.clear();
    mPrevBeta = -1.0f;
}

} // namespace bbfx
