#include "AnimationNode.h"
#include "Animator.h"

namespace bbfx {

AnimationNode::AnimationNode() {
    addEnabledPort();
}

AnimationNode::AnimationNode(const string& name) : mName(name) {
    addEnabledPort();
}

void AnimationNode::addEnabledPort() {
    // v3.5.2 Sprint S8 Lot AT — universal node enable/disable via DAG.
    // Default 1.0 (enabled). Drive with a JoystickRouterNode `toggled` /
    // `gated_value` output, a BeatTrigger, a MathNode, etc. — works on EVERY
    // node type. Disabled nodes skip update() (frozen) and gray out in the editor.
    if (mInputs.find("enabled") != mInputs.end()) return;  // idempotent
    auto* p = new AnimationPort("enabled", 1.0f);
    addPort(p, mInputs);
    setPortTooltip("enabled",
        "Universal node enable gate (>= 0.5 = enabled). Connect a JoystickRouterNode "
        "(toggle mode), BeatTrigger, MathNode, etc. to switch the whole node on/off "
        "from the DAG. Disabled = frozen update() + grayed out in editor.");
}

void AnimationNode::syncEnabledFromPort() {
    auto it = mInputs.find("enabled");
    if (it == mInputs.end()) return;
    float v = it->second->getValue();
    // Edge-detect on the PORT VALUE (not on mEnabled) : the port only drives
    // setEnabled() when its value actually changes. Otherwise a programmatic
    // setEnabled(false) (UI toggle, SetEnabledCommand, OscInputNode, …) that
    // leaves the port at its previous value would be overwritten on the next
    // tick — the original Lot AT regression. mLastEnabledPortVal init = 1.0f
    // (header) so the very first tick on a node whose ctor pushed port=0.0
    // still fires setEnabled(false) (cf. TextureShareOutputNode).
    if (v != mLastEnabledPortVal) {
        setEnabled(v >= 0.5f);
    }
    mLastEnabledPortVal = v;
}

void AnimationNode::tick() {
    syncEnabledFromPort();
    if (mEnabled) {
        onFrameAdvance();   // once-per-frame hook (update() may be re-entered by DAG propagation)
        update();
    }
}

unsigned nextCascadeApplySeq() {
    // v3.5.2 Lot AU.24 — single monotonic counter shared by Texture/Material/
    // MaterialBridge cascade. See AnimationNode.h for rationale.
    static unsigned counter = 0;
    return ++counter;
}

AnimationNode::~AnimationNode() {
    // Auto-remove from Animator if registered
    if (auto* animator = Animator::instance()) {
        animator->removeNode(this);
    }
    destroyPorts(mInputs);
    destroyPorts(mOutputs);
}

const string& AnimationNode::getName() const { return mName; }
const AnimationNode::Ports& AnimationNode::getInputs() const { return mInputs; }
const AnimationNode::Ports& AnimationNode::getOutputs() const { return mOutputs; }
AnimationNode::Ports& AnimationNode::getInputs() { return mInputs; }
AnimationNode::Ports& AnimationNode::getOutputs() { return mOutputs; }

void AnimationNode::update() {}

void AnimationNode::setListener(AnimationNodeListener* listener) {
    mListener = listener;
}

AnimationPort* AnimationNode::addInput(AnimationPort* port) {
    addPort(port, mInputs);
    return port;
}

AnimationPort* AnimationNode::addOutput(AnimationPort* port) {
    addPort(port, mOutputs);
    return port;
}

void AnimationNode::fireUpdate() {
    if (mListener) {
        mListener->notifyUpdate(this);
    }
}

void AnimationNode::addPort(AnimationPort* port, Ports& ports) {
    port->mNode = this;
    setFullName(port);
    ports[port->getName()] = port;
}

void AnimationNode::setFullName(AnimationPort* port) const {
    port->mFullName = mName + "." + port->getName();
}

void AnimationNode::destroyPorts(Ports& ports) {
    for (auto it = ports.rbegin(); it != ports.rend(); ++it) {
        delete it->second;
    }
    ports.clear();
}

} // namespace bbfx
