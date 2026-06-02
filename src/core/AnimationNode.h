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
    Ports& getInputs();
    Ports& getOutputs();

    virtual void update();

    /// v3.5.2 Sprint S8 Lot AU.11 — Once-per-frame hook, invoked by tick() right
    /// before update(). update() can be re-entered several times within one frame by
    /// the DAG propagation cascade (Animator::propagateFreshValues), so logic that
    /// must run exactly once per frame — edge detection, one-frame pulses — belongs
    /// here (e.g. JoystickRouterNode arms its edge evaluator). Default: no-op.
    virtual void onFrameAdvance() {}

    /// v3.5.2 Sprint S8 Lot AT — Per-frame tick: syncs the universal `enabled`
    /// DAG port to setEnabled(), then runs update() if enabled. The Studio main
    /// loop and headless render loop call tick() on EVERY registered node so a
    /// disabled node can still be re-enabled by an upstream signal on its port.
    /// (Non-virtual on purpose; subclasses override update(), not tick().)
    void tick();

    /// Reads the universal `enabled` input port (>= 0.5 → enabled) and calls
    /// setEnabled() if the state changed. Safe to call on disabled nodes.
    void syncEnabledFromPort();

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

    /// v3.5.2 Sprint S6 Lot W — Per-port tooltips for InspectorPanel hover help.
    /// Set in node ctors for the user-facing ports. Empty = no tooltip.
    void setPortTooltip(const string& portName, const string& tip) {
        mPortTooltips[portName] = tip;
    }
    const string& getPortTooltip(const string& portName) const {
        static const string kEmpty;
        auto it = mPortTooltips.find(portName);
        return (it != mPortTooltips.end()) ? it->second : kEmpty;
    }

protected:
    string mName;
    Ports mInputs;
    Ports mOutputs;
    bool mEnabled = true;
    bool mUserVisible = true;
    bool mLocked = false;
    float mLastEnabledPortVal = 1.0f;   // v3.5.2 Sprint S8 Lot AT — last seen `enabled` port value (edge detect)

    /// v3.5.2 Sprint S8 Lot AT — adds the universal `enabled` input port (default 1.0).
    /// Called from the AnimationNode ctors. Idempotent: a subclass may NOT re-add it.
    void addEnabledPort();

    AnimationPort* addInput(AnimationPort* port);
    AnimationPort* addOutput(AnimationPort* port);
    void fireUpdate();

    ParamSpec* mParamSpec = nullptr;
    std::map<string, string> mPortTooltips;
    AnimationNodeListener* getListener() const { return mListener; }

    friend class Animator; // for renameNode() access to mName

private:
    AnimationNodeListener* mListener = nullptr;
    void addPort(AnimationPort* port, Ports& ports);
    void setFullName(AnimationPort* port) const;
    void destroyPorts(Ports& ports);
};

/// v3.5.2 Lot AU.24 — shared monotonic counter for the Pattern 4 cascade
/// (TextureNode + MaterialNode + MaterialBridgeNode targeting a SceneObjectNode).
/// Per-class static counters meant the "last connected wins" rule was not
/// well-ordered across classes — peers of different types could collide on the
/// same seq value depending on the relative count of past instantiations,
/// breaking the cross-class hand-back. One counter, one monotonic order.
unsigned nextCascadeApplySeq();

} // namespace bbfx
