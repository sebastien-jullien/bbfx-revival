#pragma once

#include "../bbfx.h"
#include "AnimationPort.h"
#include "ParamSpec.h"
#include <map>

namespace bbfx {

class AnimationNode; // forward

class AnimationNodeListener {
public:
    virtual ~AnimationNodeListener() = default;
    virtual void notifyUpdate(AnimationNode* node) = 0;
};

class AnimationNode {
public:
    AnimationNode();
    explicit AnimationNode(const string& name);
    virtual ~AnimationNode();

    const string& getName() const;
    virtual std::string getTypeName() const { return "AnimationNode"; }

    using Ports = std::map<string, AnimationPort*>;
    const Ports& getInputs() const;
    const Ports& getOutputs() const;

    virtual void update();

    /// Enable/disable the node. Disabled nodes skip update() and appear grayed out in the editor.
    bool isEnabled() const { return mEnabled; }
    virtual void setEnabled(bool en) { mEnabled = en; }

    /// User-controlled visibility (distinct from enable). Affects OGRE rendering only, not DAG.
    bool isUserVisible() const { return mUserVisible; }
    virtual void setUserVisible(bool v) { mUserVisible = v; }

    /// Lock prevents selection and transformation via viewport picking/gizmo.
    bool isLocked() const { return mLocked; }
    void setLocked(bool l) { mLocked = l; }

    /// Called before deletion to destroy OGRE objects (Entity, SceneNode, Material, etc.)
    /// Override in FX nodes that create OGRE resources.
    virtual void cleanup() {}

    /// Called when a link to/from this node is created or deleted.
    /// Override in FX nodes to react immediately (e.g. resolve target changes).
    virtual void onLinkChanged() {}

    void setListener(AnimationNodeListener* listener);

    /// Optional typed parameter spec. If set, Inspector generates widgets automatically.
    void setParamSpec(ParamSpec* spec) { mParamSpec = spec; }
    ParamSpec* getParamSpec() const { return mParamSpec; }

protected:
    string mName;
    Ports mInputs;
    Ports mOutputs;
    bool mEnabled = true;
    bool mUserVisible = true;
    bool mLocked = false;

    AnimationPort* addInput(AnimationPort* port);
    AnimationPort* addOutput(AnimationPort* port);
    void fireUpdate();

    ParamSpec* mParamSpec = nullptr;
    AnimationNodeListener* getListener() const { return mListener; }

    friend class Animator; // for renameNode() access to mName

private:
    AnimationNodeListener* mListener = nullptr;
    void addPort(AnimationPort* port, Ports& ports);
    void setFullName(AnimationPort* port) const;
    void destroyPorts(Ports& ports);
};

} // namespace bbfx
