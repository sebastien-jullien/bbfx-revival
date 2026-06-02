#include "VideoSlicerNode.h"
#include "../../core/Animator.h"
#include "../../video/TheoraClipNode.h"
#include "../../video/TheoraClip.h"
#include <algorithm>
#include <cmath>

namespace bbfx {

VideoSlicerNode::VideoSlicerNode(const std::string& name)
    : AnimationNode(name)
{
    ParamDef inDef;
    inDef.name = "in_point"; inDef.label = "In Point";
    inDef.type = ParamType::FLOAT;
    inDef.floatVal = 0.0f; inDef.minVal = 0.0f; inDef.maxVal = 1.0f;
    mSpec.addParam(inDef);

    ParamDef outDef;
    outDef.name = "out_point"; outDef.label = "Out Point";
    outDef.type = ParamType::FLOAT;
    outDef.floatVal = 1.0f; outDef.minVal = 0.0f; outDef.maxVal = 1.0f;
    mSpec.addParam(outDef);

    ParamDef autoDef;
    autoDef.name = "auto_play"; autoDef.label = "Auto Play";
    autoDef.type = ParamType::BOOL; autoDef.boolVal = true;
    mSpec.addParam(autoDef);

    ParamDef speedDef;
    speedDef.name = "playback_speed"; speedDef.label = "Playback Speed";
    speedDef.type = ParamType::FLOAT;
    speedDef.floatVal = 1.0f; speedDef.minVal = -4.0f; speedDef.maxVal = 4.0f;
    mSpec.addParam(speedDef);

    ParamDef matMir;
    matMir.name = "material_out"; matMir.label = "Material (out)";
    matMir.type = ParamType::STRING; matMir.stringVal = "";
    matMir.readOnly = true;
    matMir.tooltip = "Read-only: sliced/scrubbed video material name. Routable to MaterialBridgeNode.";
    mSpec.addParam(matMir);

    setParamSpec(&mSpec);

    addInput(new AnimationPort("dt",        1.0f / 60.0f));
    addInput(new AnimationPort("clip",      0.0f));   // entity-link
    addInput(new AnimationPort("playhead", -1.0f));   // -1 = no override

    addOutput(new AnimationPort("playhead_normalized", 0.0f));
    addOutput(new AnimationPort("material_ready",      0.0f));

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("clip",               "Entity-link to the source TheoraClipNode (entity output).");
    setPortTooltip("playhead",           "Direct seek time in seconds. -1 = let in_point/out_point loop drive playback.");
    setPortTooltip("playhead_normalized","Current playhead within the slice (in_point..out_point) mapped to 0..1.");
    setPortTooltip("material_ready",     "Pulses 1.0 when the slice material is bound (route -> MaterialBridge).");
}

void VideoSlicerNode::resolveClip() {
    auto* animator = Animator::instance();
    if (!animator) { mClip = nullptr; mPrevClipNode = nullptr; return; }
    auto& in = getInputs();
    auto it = in.find("clip");
    if (it == in.end()) return;
    auto sources = animator->getSourceNodes(it->second);
    AnimationNode* found = nullptr;
    for (auto* s : sources) {
        if (auto* tcn = dynamic_cast<TheoraClipNode*>(s)) { found = tcn; break; }
    }
    if (found != mPrevClipNode) {
        mPrevClipNode = found;
        mClip = found ? dynamic_cast<TheoraClipNode*>(found)->getClip() : nullptr;
    }

    // Detect whether the playhead port has any source (= scrub mode).
    auto itP = in.find("playhead");
    if (itP != in.end()) {
        auto pSrc = animator->getSourceNodes(itP->second);
        mPlayheadConnected = !pSrc.empty();
    }
}

void VideoSlicerNode::update() {
    if (!mEnabled) return;

    resolveClip();

    auto* inP = mSpec.getParam("in_point");
    auto* outP = mSpec.getParam("out_point");
    auto* autoP = mSpec.getParam("auto_play");
    auto* speedP = mSpec.getParam("playback_speed");
    if (inP)    mInPoint  = std::clamp(inP->floatVal, 0.0f, 1.0f);
    if (outP)   mOutPoint = std::clamp(outP->floatVal, 0.0f, 1.0f);
    if (autoP)  mAutoPlay = autoP->boolVal;
    if (speedP) mPlaybackSpeed = std::clamp(speedP->floatVal, -4.0f, 4.0f);

    // in_point > out_point swap defensively
    if (mInPoint > mOutPoint) std::swap(mInPoint, mOutPoint);

    auto& in = getInputs();
    auto& out = getOutputs();

    if (!mClip) {
        if (out.count("material_ready")) out.at("material_ready")->setValue(0.0f);
        return;
    }

    // in_point/out_point sont des fractions NORMALISÉES [0,1] du clip entier, mais
    // TheoraClip::setTime() attend des SECONDES. On convertit via la durée réelle.
    // (M3 — sans ça, t∈[0,1] adressait seulement la 1ʳᵉ seconde de tout clip.)
    float dur = mClip->getDuration();
    if (dur <= 0.0f) dur = 1.0f;   // fallback durée inconnue : comportement legacy

    // Mode scrub (playhead port connected): seek directly each frame.
    if (mPlayheadConnected) {
        float ph = std::clamp(in.at("playhead")->getValue(), 0.0f, 1.0f);
        float t = mInPoint + ph * (mOutPoint - mInPoint);
        mInternalPlayhead = ph;
        mClip->setTime(t * dur);
    } else if (mAutoPlay) {
        // Auto-play within [in, out] segment, looping with playback_speed sign.
        float dt = in.at("dt")->getValue();
        if (dt <= 0.0f) dt = mPrevDt;
        else mPrevDt = dt;
        float segLen = std::max(0.0001f, mOutPoint - mInPoint);
        float advance = mPlaybackSpeed * dt / segLen;
        mInternalPlayhead += advance;
        // wrap
        if (mInternalPlayhead >= 1.0f) mInternalPlayhead -= std::floor(mInternalPlayhead);
        if (mInternalPlayhead <  0.0f) mInternalPlayhead += std::ceil(-mInternalPlayhead);
        float t = mInPoint + mInternalPlayhead * segLen;
        mClip->setTime(t * dur);
    } // else freeze (no time update)

    mLastMaterialName = mClip->getMaterialName();
    auto* mirror = mSpec.getParam("material_out");
    if (mirror) mirror->stringVal = mLastMaterialName;

    if (out.count("playhead_normalized")) out.at("playhead_normalized")->setValue(mInternalPlayhead);
    if (out.count("material_ready"))      out.at("material_ready")->setValue(1.0f);

    fireUpdate();
}

} // namespace bbfx
