#include "PrimitiveNodes.h"
#include "Animator.h"
#include "../studio/nodes/SceneObjectNode.h"
#include <OgreSkeletonInstance.h>
#include <OgreBone.h>
#include <OgreRTShaderSystem.h>
#include <OgreShaderExHardwareSkinning.h>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace bbfx {

// ── LuaAnimationNode ────────────────────────────────────────────────────────

LuaAnimationNode::LuaAnimationNode(const string& name, sol::function update)
    : AnimationNode(name), mUpdateHook(std::move(update)) {
    // Entity input port — multi-link: accepts N SceneObjectNode connections
    AnimationNode::addInput(new AnimationPort("entity", 0.0f, true));

    // ParamSpec with target_entity (same pattern as FX nodes)
    ParamDef targetDef;
    targetDef.name = "target_entity";
    targetDef.type = ParamType::STRING;
    mSpec.addParam(targetDef);
    setParamSpec(&mSpec);
}

LuaAnimationNode::~LuaAnimationNode() = default;

void LuaAnimationNode::addInput(const string& name) {
    AnimationNode::addInput(new AnimationPort(name, 0.0f));
}

void LuaAnimationNode::addOutput(const string& name) {
    AnimationNode::addOutput(new AnimationPort(name, 0.0f));
}

void LuaAnimationNode::update() {
    if (!mUpdateHook.valid()) return;

    if (mTargetNodeNames.size() <= 1) {
        // Single target or no target: call once (original behavior)
        auto result = mUpdateHook(this);
        if (!result.valid()) {
            sol::error err = result;
            cerr << "LuaAnimationNode update error: " << err.what() << endl;
        }
    } else {
        // Multi-target: call the update hook once per target,
        // temporarily setting mTargetNodeName to each target.
        // This way, getTargetSceneNode() returns the correct target
        // for scripts using the single-target API.
        std::string saved = mTargetNodeName;
        for (auto& name : mTargetNodeNames) {
            mTargetNodeName = name;
            auto result = mUpdateHook(this);
            if (!result.valid()) {
                sol::error err = result;
                cerr << "LuaAnimationNode update error on target '" << name << "': " << err.what() << endl;
                continue;
            }
        }
        mTargetNodeName = saved;
    }
}

void LuaAnimationNode::onLinkChanged() {
    // Build target list from the DAG graph (source of truth)
    mTargetNodeNames.clear();
    auto* animator = Animator::instance();
    if (animator) {
        auto& inputs = getInputs();
        auto it = inputs.find("entity");
        if (it != inputs.end()) {
            auto sources = animator->getSourceNodes(it->second);
            for (auto* srcNode : sources) {
                if (srcNode && !srcNode->getName().empty())
                    mTargetNodeNames.push_back(srcNode->getName());
            }
        }
    }
    mTargetNodeName = mTargetNodeNames.empty() ? "" : mTargetNodeNames.front();
}

Ogre::SceneNode* LuaAnimationNode::getTargetSceneNode() const {
    if (mTargetNodeName.empty()) return nullptr;
    auto* animator = Animator::instance();
    if (!animator) return nullptr;
    auto* node = animator->getRegisteredNode(mTargetNodeName);
    if (!node) return nullptr;
    auto* sceneObj = dynamic_cast<SceneObjectNode*>(node);
    return sceneObj ? sceneObj->getSceneNode() : nullptr;
}

std::vector<Ogre::SceneNode*> LuaAnimationNode::getTargetSceneNodes() const {
    std::vector<Ogre::SceneNode*> result;
    auto* animator = Animator::instance();
    if (!animator) return result;
    for (auto& name : mTargetNodeNames) {
        auto* node = animator->getRegisteredNode(name);
        if (!node) continue;
        auto* sceneObj = dynamic_cast<SceneObjectNode*>(node);
        if (sceneObj && sceneObj->getSceneNode())
            result.push_back(sceneObj->getSceneNode());
    }
    return result;
}

// ── AnimableValuePort ───────────────────────────────────────────────────────

AnimableValuePort::AnimableValuePort(const string& name, Ogre::AnimableValuePtr target)
    : AnimationPort(name), mTarget(target) {}

AnimableValuePort::~AnimableValuePort() = default;

void AnimableValuePort::setValue(Ogre::Real value) {
    AnimationPort::setValue(value);
    if (mTarget) {
        mTarget->setValue(value);
    }
}

// ── AnimableObjectNode ──────────────────────────────────────────────────────

AnimableObjectNode::AnimableObjectNode(Ogre::AnimableObject* animable) {
    const auto& names = animable->getAnimableValueNames();
    for (const auto& name : names) {
        auto avp = animable->createAnimableValue(name);
        addInput(new AnimableValuePort(name, avp));
    }
}

// ── AnimationStateNode ──────────────────────────────────────────────────────

AnimationStateNode::AnimationStateNode(const std::string& name, Ogre::SceneManager* scene)
    : AnimationNode(name), mScene(scene) {
    // Drive ports. `time` linked → absolute scrub ; sinon auto-advance via dt*speed.
    mTimePort  = addInput(new AnimationPort("time", 0.0f));
    mDtPort    = addInput(new AnimationPort("dt", 0.0f));
    mSpeedPort = addInput(new AnimationPort("speed", 1.0f));
    mPlayPort  = addInput(new AnimationPort("play", 1.0f));
    addInput(new AnimationPort("entity", 0.0f, true)); // entity-link (Pattern 1)

    ParamDef animDef; animDef.name = "animation_name"; animDef.label = "Animation";
    animDef.type = ParamType::STRING; animDef.stringVal = "";
    animDef.tooltip = "Nom du clip à jouer (vide = premier disponible). Voir available_animations.";
    mSpec.addParam(animDef);

    ParamDef loopDef; loopDef.name = "loop"; loopDef.label = "Loop";
    loopDef.type = ParamType::BOOL; loopDef.boolVal = true;
    mSpec.addParam(loopDef);

    ParamDef tgtDef; tgtDef.name = "target_entity"; tgtDef.type = ParamType::STRING;
    tgtDef.readOnly = true; tgtDef.tooltip = "Cible résolue via le port entity-link (read-only).";
    mSpec.addParam(tgtDef);

    ParamDef availDef; availDef.name = "available_animations"; availDef.type = ParamType::STRING;
    availDef.readOnly = true; availDef.tooltip = "Clips d'animation disponibles sur le mesh cible (read-only).";
    mSpec.addParam(availDef);

    setParamSpec(&mSpec);
}

AnimationStateNode::~AnimationStateNode() { cleanup(); }

Ogre::Entity* AnimationStateNode::resolveEntity(std::string& outNodeName) {
    outNodeName.clear();
    auto* animator = Animator::instance();
    if (!animator) return nullptr;
    auto& inputs = getInputs();
    auto it = inputs.find("entity");
    if (it == inputs.end()) return nullptr;
    for (auto* src : animator->getSourceNodes(it->second)) {
        auto* so = dynamic_cast<SceneObjectNode*>(src);
        if (so && so->getEntity()) { outNodeName = so->getName(); return so->getEntity(); }
    }
    return nullptr;
}

void AnimationStateNode::disableActive() {
    if (mActiveAnim.empty()) return;
    std::string n;
    if (Ogre::Entity* ent = resolveEntity(n)) {
        Ogre::AnimationStateSet* set = ent->getAllAnimationStates();
        if (set && set->hasAnimationState(mActiveAnim))
            set->getAnimationState(mActiveAnim)->setEnabled(false);
    }
    mActiveAnim.clear();
}

void AnimationStateNode::update() {
    std::string nodeName;
    Ogre::Entity* ent = resolveEntity(nodeName);

    auto setMirror = [&](const char* k, const std::string& v) {
        if (auto* p = mSpec.getParam(k)) p->stringVal = v;
    };

    // target_entity reflète l'entité résolue (même si elle n'a pas d'animation).
    setMirror("target_entity", ent ? nodeName : "");

    // Pas de cible animée → relâcher le clip en cours et vider la liste.
    Ogre::AnimationStateSet* set = ent ? ent->getAllAnimationStates() : nullptr;
    if (!set || set->getAnimationStates().empty()) {
        disableActive();
        setMirror("available_animations", "");
        fireUpdate();
        return;
    }

    // Available clips (read-only mirror).
    std::string joined;
    for (const auto& kv : set->getAnimationStates()) {
        if (!joined.empty()) joined += ", ";
        joined += kv.first;
    }
    setMirror("available_animations", joined);

    // Pick the requested clip, or the first available if unset/unknown.
    std::string want;
    if (auto* p = mSpec.getParam("animation_name")) want = p->stringVal;
    if (want.empty() || !set->hasAnimationState(want))
        want = set->getAnimationStates().begin()->first;

    // Clip switch (or entity changed) → disable the previous one, reset clock.
    if (want != mActiveAnim) { disableActive(); mActiveAnim = want; mAccumTime = 0.0f; }

    Ogre::AnimationState* state = set->getAnimationState(want);
    bool loop = true;
    if (auto* p = mSpec.getParam("loop")) loop = p->boolVal;
    state->setLoop(loop);
    state->setEnabled(true);

    // Prépare l'entité pour le skinning MATÉRIEL via RTSS : ajoute le sous-état
    // hardware-skinning à ses matériaux pour que le shader généré applique les
    // matrices d'os. Sans ça, le mesh reste en bind pose sous RTShaderSystem.
    // (Idempotent par entité ; l'ancienne entité détruite est remplacée au resolve.)
    if (ent != mPreparedEntity) {
        Ogre::RTShader::HardwareSkinningFactory::prepareEntityForSkinning(ent);
        if (auto* sg = Ogre::RTShader::ShaderGenerator::getSingletonPtr())
            sg->invalidateScheme(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME);
        mPreparedEntity = ent;
    }

    const float length = state->getLength();
    auto wrap = [&](float t) -> float {
        if (length <= 0.0f) return 0.0f;
        if (loop) { t = std::fmod(t, length); if (t < 0.0f) t += length; return t; }
        return std::clamp(t, 0.0f, length);
    };

    // Time source : linked `time` port = absolute scrub ; sinon auto-advance.
    bool timeLinked = false;
    if (mTimePort) {
        auto* an = Animator::instance();
        timeLinked = an && !an->getSourceNodes(mTimePort).empty();
    }

    if (timeLinked) {
        mAccumTime = wrap(mTimePort->getValue());
        state->setTimePosition(mAccumTime);
    } else {
        bool playing = mPlayPort ? (mPlayPort->getValue() >= 0.5f) : true;
        if (playing) {
            float speed = mSpeedPort ? mSpeedPort->getValue() : 1.0f;
            float dt    = mDtPort ? mDtPort->getValue() : 0.0f;
            mAccumTime  = wrap(mAccumTime + dt * speed);
            state->setTimePosition(mAccumTime);
        }
    }

    // Applique EXPLICITEMENT les états d'animation activés au squelette à chaque
    // tick, puis force le recalcul des matrices d'os de skinning.
    if (ent->hasSkeleton()) {
        ent->getSkeleton()->setAnimationState(*set);
        ent->getSkeleton()->_updateTransforms();
        // CLEF du bug « ninja immobile » : OGRE saute cacheBoneMatrices() quand une
        // requête de bounding-box plus tôt dans la frame a déjà « consommé » le numéro
        // de frame d'animation (garde mFrameAnimationLastUpdated == DirtyFrameNumber).
        // Les matrices d'os transmises au skinning restaient figées en bind pose.
        // _notifyManualBonesDirty() force getManualBonesDirty()==true → _updateAnimation
        // recalcule réellement les matrices à partir du squelette posé ci-dessus.
        ent->getSkeleton()->_notifyManualBonesDirty();
        ent->_updateAnimation();
    }

    fireUpdate();
}

void AnimationStateNode::cleanup() {
    disableActive();
    mPreparedEntity = nullptr;   // le sous-état RTSS vit dans le matériau de l'entité (libéré avec elle)
}

// ── RootTimeNode ────────────────────────────────────────────────────────────

RootTimeNode* RootTimeNode::sInstance = nullptr;

RootTimeNode::RootTimeNode() : AnimationNode("time") {
    assert(!sInstance);
    sInstance = this;
    mFrameTimePort = addOutput(new AnimationPort("dt", 0.0f));
    mTotalTimePort = addOutput(new AnimationPort("total", 0.0f));
    mBeatPort      = addOutput(new AnimationPort("beat", 0.0f));
    mBeatFracPort  = addOutput(new AnimationPort("beatFrac", 0.0f));
    mBPMPort       = addInput(new AnimationPort("bpm", 120.0f));
    mLastTime = std::chrono::steady_clock::now();
}

RootTimeNode::~RootTimeNode() {
    sInstance = nullptr;
}

RootTimeNode* RootTimeNode::instance() {
    return sInstance;
}

void RootTimeNode::reset() {
    mTotalTime = 0.0f;
    mLastTime = std::chrono::steady_clock::now();
    mFrameTimePort->setValue(0.0f);
    mTotalTimePort->setValue(0.0f);
    mBeatPort->setValue(0.0f);
    mBeatFracPort->setValue(0.0f);
    fireUpdate();
}

void RootTimeNode::resume() {
    mLastTime = std::chrono::steady_clock::now();
}

void RootTimeNode::seekTo(float totalTime) {
    mTotalTime = totalTime;
    mLastTime = std::chrono::steady_clock::now();

    float bpm = mBPMPort->getValue();
    if (bpm > 0.0f) {
        float beatsPerSec = bpm / 60.0f;
        float currentBeat = mTotalTime * beatsPerSec;
        mBeatPort->setValue(currentBeat);
        mBeatFracPort->setValue(currentBeat - std::floor(currentBeat));
    }
    mTotalTimePort->setValue(mTotalTime);
    mFrameTimePort->setValue(0.0f);
    fireUpdate();
}

Ogre::Real RootTimeNode::getTotalTime() const {
    return mTotalTime;
}

void RootTimeNode::setBPM(float bpm) {
    if (bpm <= 0.0f) return;
    float oldBPM = mBPMPort->getValue();
    if (std::abs(bpm - oldBPM) > 0.01f && oldBPM > 0.0f) {
        // Adjust totalTime so beat count stays continuous (no jump)
        float currentBeat = mTotalTime * oldBPM / 60.0f;
        mTotalTime = currentBeat * 60.0f / bpm;
        mTotalTimePort->setValue(mTotalTime);
    }
    mBPMPort->setValue(bpm);
}

float RootTimeNode::getBPM() const {
    return mBPMPort->getValue();
}

void RootTimeNode::update() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<float>(now - mLastTime).count();
    mLastTime = now;
    if (elapsed > 0.1f) elapsed = 0.1f;  // Clamp max dt to prevent time jumps
    mTotalTime += elapsed;
    mFrameTimePort->setValue(elapsed);
    mTotalTimePort->setValue(mTotalTime);

    // Beat-synced outputs: beat number and fractional beat (0..1 sawtooth)
    float bpm = mBPMPort->getValue();
    if (bpm > 0.0f) {
        float beatsPerSec = bpm / 60.0f;
        float currentBeat = mTotalTime * beatsPerSec;
        mBeatPort->setValue(currentBeat);
        mBeatFracPort->setValue(currentBeat - std::floor(currentBeat));
    }
    fireUpdate();
}

// ── AccumulatorNode ─────────────────────────────────────────────────────────

AccumulatorNode::AccumulatorNode() : AnimationNode("accumulator") {
    mInput = addInput(new AnimationPort("delta", 0.0f));
    // D8 — bornes/wrap/reset (défaut min==max==0 ⇒ accumulation non bornée = legacy).
    mReset = addInput(new AnimationPort("reset", 0.0f));
    mMin   = addInput(new AnimationPort("min", 0.0f));
    mMax   = addInput(new AnimationPort("max", 0.0f));
    mWrap  = addInput(new AnimationPort("wrap", 0.0f));
    mSum   = addOutput(new AnimationPort("sum", 0.0f));
}

void AccumulatorNode::update() {
    Ogre::Real delta = mInput->getValue();
    Ogre::Real current = mSum->getValue();

    // D8 — reset sur front montant : remet à min (ou 0 si pas de bornes).
    bool reset = (mReset->getValue() > 0.5f);
    Ogre::Real lo = mMin->getValue();
    Ogre::Real hi = mMax->getValue();
    bool bounded = (hi > lo);
    if (reset && !mPrevReset) {
        current = bounded ? lo : Ogre::Real(0);
        mPrevReset = true;
        mSum->setValue(current);
        fireUpdate();
        return;
    }
    mPrevReset = reset;

    Ogre::Real result = current + delta;
    if (bounded) {
        Ogre::Real range = hi - lo;
        if (mWrap->getValue() > 0.5f) {
            // wrap modulo [lo,hi)
            Ogre::Real t = std::fmod(result - lo, range);
            if (t < 0) t += range;
            result = lo + t;
        } else {
            // clamp [lo,hi]
            if (result < lo) result = lo;
            else if (result > hi) result = hi;
        }
    }
    mSum->setValue(result);
    fireUpdate();  // D8 — l'ancienne version ne notifiait pas l'aval
}

} // namespace bbfx
