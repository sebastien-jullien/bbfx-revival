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

namespace Ogre { class Entity; }

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
    /// Returns the name of the first linked target SceneObjectNode (empty if none).
    const std::string& getTargetNodeName() const { return mTargetNodeName; }
    /// Returns all linked target node names.
    const std::vector<std::string>& getTargetNodeNames() const { return mTargetNodeNames; }
    /// Resolves first target_entity → SceneObjectNode → OGRE SceneNode (nullptr if unlinked).
    Ogre::SceneNode* getTargetSceneNode() const;
    /// Resolves all target entities → SceneNodes.
    std::vector<Ogre::SceneNode*> getTargetSceneNodes() const;
protected:
    sol::function mUpdateHook;
    std::string mSource; // Lua source code (for serialization)
    ParamSpec mSpec;
    std::string mTargetNodeName;                // first target (compat)
    std::vector<std::string> mTargetNodeNames;  // all targets
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

/// Plays a skeletal / vertex animation of an upstream animated mesh.
/// Connect a SceneObjectNode loading a rigged mesh (ninja, robot, fish, …) to
/// the `entity` port, pick the clip via `animation_name` (empty = first
/// available), then drive playback either by linking the `time` port
/// (absolute scrub, e.g. from RootTime/beat) or by auto-advance via `dt`*`speed`
/// when `play >= 0.5`. The list of available clips is mirrored read-only in
/// `available_animations`. Uses the OGRE entity's AnimationState (never
/// SceneManager::getAnimation — that was the v1 crash source).
class AnimationStateNode : public AnimationNode {
public:
    AnimationStateNode(const std::string& name, Ogre::SceneManager* scene);
    ~AnimationStateNode() override;
    void update() override;
    void cleanup() override;
    std::string getTypeName() const override { return "AnimationStateNode"; }
protected:
    Ogre::SceneManager* mScene = nullptr;
    ParamSpec mSpec;
    AnimationPort* mTimePort  = nullptr;
    AnimationPort* mDtPort    = nullptr;
    AnimationPort* mSpeedPort = nullptr;
    AnimationPort* mPlayPort  = nullptr;
    std::string mActiveAnim;     // currently-enabled clip (to disable on switch/cleanup)
    float mAccumTime = 0.0f;     // auto-advance accumulator (seconds)
    Ogre::Entity* mPreparedEntity = nullptr; // entité préparée pour le hardware-skinning RTSS

    /// Resolves the upstream SceneObjectNode entity via the `entity` link.
    Ogre::Entity* resolveEntity(std::string& outNodeName);
    /// Disables the currently-active AnimationState if it still exists.
    void disableActive();
};

class RootTimeNode : public AnimationNode {
public:
    RootTimeNode();
    virtual ~RootTimeNode();
    static RootTimeNode* instance();
    void update() override;
    void reset();
    void resume();                    // Reset mLastTime without resetting mTotalTime (for unpause)
    void seekTo(float totalTime);     // Seek to a specific time position
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
    AnimationPort* mInput;   // delta
    AnimationPort* mSum;      // sortie accumulée
    AnimationPort* mReset;    // D8 — front montant : remet sum à min (ou 0)
    AnimationPort* mMin;      // D8 — borne basse (active si min<max)
    AnimationPort* mMax;      // D8 — borne haute (active si min<max)
    AnimationPort* mWrap;     // D8 — 0 = clamp dans [min,max], 1 = wrap modulo [min,max]
    bool mPrevReset = false;  // edge-detect du reset
};

} // namespace bbfx
