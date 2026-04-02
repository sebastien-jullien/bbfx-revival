#pragma once

#include "../bbfx.h"
#include "AnimationNode.h"

#include <sol/sol.hpp>

#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreAnimation.h>
#include <OgreAnimationState.h>
#include <OgreController.h>
#include <OgreControllerManager.h>

#include <chrono>

namespace bbfx {

class LuaAnimationNode : public AnimationNode {
public:
    LuaAnimationNode(const string& name, sol::function update);
    virtual ~LuaAnimationNode();
    void addInput(const string& name);
    void addOutput(const string& name);
    void update() override;
    void onLinkChanged() override;
    void setUpdateFunction(sol::function fn) { mUpdateHook = std::move(fn); }
    void setSource(const std::string& src) { mSource = src; }
    const std::string& getSource() const { return mSource; }
    std::string getTypeName() const override { return "LuaAnimationNode"; }
    /// Returns the name of the linked target SceneObjectNode (empty if none).
    const std::string& getTargetNodeName() const { return mTargetNodeName; }
    /// Resolves target_entity → SceneObjectNode → OGRE SceneNode (nullptr if unlinked).
    Ogre::SceneNode* getTargetSceneNode() const;
protected:
    sol::function mUpdateHook;
    std::string mSource; // Lua source code (for serialization)
    ParamSpec mSpec;
    std::string mTargetNodeName;
};

class AnimableValuePort : public AnimationPort {
public:
    AnimableValuePort(const string& name, Ogre::AnimableValuePtr target);
    virtual ~AnimableValuePort();
    void setValue(Ogre::Real value) override;
protected:
    Ogre::AnimableValuePtr mTarget;
};

class AnimableObjectNode : public AnimationNode {
public:
    explicit AnimableObjectNode(Ogre::AnimableObject* animable);
};

class AnimationStateNode : public AnimationNode {
public:
    explicit AnimationStateNode(Ogre::SceneManager* scene, Ogre::AnimationState* state);
    virtual ~AnimationStateNode();
    void update() override;
protected:
    AnimationPort* mTimePort;
    Ogre::AnimationState* mState;
    Ogre::Animation* mAnimation;
    void prepare();
};

class RootTimeNode : public AnimationNode {
public:
    RootTimeNode();
    virtual ~RootTimeNode();
    static RootTimeNode* instance();
    void update() override;
    void reset();
    Ogre::Real getTotalTime() const;
    void setBPM(float bpm);
    float getBPM() const;
    std::string getTypeName() const override { return "RootTimeNode"; }
protected:
    static RootTimeNode* sInstance;
    AnimationPort* mFrameTimePort;
    AnimationPort* mTotalTimePort;
    AnimationPort* mBeatPort;      // current beat number (total * bpm / 60)
    AnimationPort* mBPMPort;       // BPM value (input — set from Timeline)
    AnimationPort* mBeatFracPort;  // fractional beat (0..1 sawtooth synced to beat)
    Ogre::Real mTotalTime = 0.0f;
    std::chrono::steady_clock::time_point mLastTime;
};

class AccumulatorNode : public AnimationNode {
public:
    AccumulatorNode();
    void update() override;
    std::string getTypeName() const override { return "AccumulatorNode"; }
protected:
    AnimationPort* mInput;
    AnimationPort* mSum;
};

} // namespace bbfx
