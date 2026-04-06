#include "PrimitiveNodes.h"
#include "Animator.h"
#include "../studio/nodes/SceneObjectNode.h"
#include <sstream>

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

AnimationStateNode::AnimationStateNode(Ogre::SceneManager* scene, Ogre::AnimationState* state)
    : AnimationNode(state->getAnimationName()),
      mState(state),
      mAnimation(scene->getAnimation(state->getAnimationName())) {
    mTimePort = addInput(new AnimationPort("time", 0.0f));
}

AnimationStateNode::~AnimationStateNode() = default;

void AnimationStateNode::update() {
    Ogre::Real t = mTimePort->getValue();
    mState->setTimePosition(t);
    mState->setEnabled(true);
}

void AnimationStateNode::prepare() {
    // In OGRE 14, animation state management handles bone reset internally
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
    if (bpm > 0.0f) mBPMPort->setValue(bpm);
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
    mSum = addOutput(new AnimationPort("sum", 0.0f));
}

void AccumulatorNode::update() {
    Ogre::Real delta = mInput->getValue();
    Ogre::Real current = mSum->getValue();
    mSum->setValue(current + delta);
}

} // namespace bbfx
