#include "AnimationPort.h"

namespace bbfx {

AnimationPort::AnimationPort(const string& name, Ogre::Real value)
    : mName(name), mValue(value) {}

AnimationPort::~AnimationPort() = default;

AnimationNode* AnimationPort::getNode() const { return mNode; }
const string& AnimationPort::getName() const { return mName; }
const string& AnimationPort::getFullName() const { return mFullName; }
Ogre::Real AnimationPort::getValue() const { return mValue; }

void AnimationPort::setValue(Ogre::Real value) {
    mValue = value;
}

} // namespace bbfx
