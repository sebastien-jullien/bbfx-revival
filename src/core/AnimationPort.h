#pragma once

#include "../bbfx.h"
#include <OgrePrerequisites.h>

namespace bbfx {

class AnimationNode; // forward

class AnimationPort {
    friend class AnimationNode;
    friend class Animator; // for renameNode() access to mFullName
public:
    AnimationPort(const string& name = "", Ogre::Real value = 0.0f, bool multiLink = false);
    virtual ~AnimationPort();

    AnimationNode* getNode() const;
    const string& getName() const;
    const string& getFullName() const;

    Ogre::Real getValue() const;
    virtual void setValue(Ogre::Real value);

    /// Multi-link ports accept N incoming connections without overwriting.
    bool isMultiLink() const { return mMultiLink; }

protected:
    AnimationNode* mNode = nullptr;
    string mName;
    string mFullName;
    Ogre::Real mValue = 0.0f;
    bool mMultiLink = false;
};

} // namespace bbfx
