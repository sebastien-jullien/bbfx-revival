#include "AnimationNode.h"

namespace bbfx {

AnimationNode::AnimationNode() = default;

AnimationNode::AnimationNode(const string& name) : mName(name) {}

AnimationNode::~AnimationNode() {
    destroyPorts(mInputs);
    destroyPorts(mOutputs);
}

const string& AnimationNode::getName() const { return mName; }
const AnimationNode::Ports& AnimationNode::getInputs() const { return mInputs; }
const AnimationNode::Ports& AnimationNode::getOutputs() const { return mOutputs; }

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
