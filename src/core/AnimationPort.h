#pragma once

#include "../bbfx.h"
#include <OgrePrerequisites.h>

namespace bbfx {

class AnimationNode; // forward

class AnimationPort {
    friend class AnimationNode;
public:
    AnimationPort(const string& name = "", Ogre::Real value = 0.0f);
    virtual ~AnimationPort();

    AnimationNode* getNode() const;
    const string& getName() const;
    const string& getFullName() const;

    Ogre::Real getValue() const;
    virtual void setValue(Ogre::Real value);

protected:
    AnimationNode* mNode = nullptr;
    string mName;
    string mFullName;
    Ogre::Real mValue = 0.0f;
};

} // namespace bbfx
