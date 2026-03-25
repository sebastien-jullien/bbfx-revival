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
protected:
    sol::function mUpdateHook;
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
protected:
    static RootTimeNode* sInstance;
    AnimationPort* mFrameTimePort;
    AnimationPort* mTotalTimePort;
    Ogre::Real mTotalTime = 0.0f;
    std::chrono::steady_clock::time_point mLastTime;
};

class AccumulatorNode : public AnimationNode {
public:
    AccumulatorNode();
    void update() override;
protected:
    AnimationPort* mInput;
    AnimationPort* mSum;
};

} // namespace bbfx
