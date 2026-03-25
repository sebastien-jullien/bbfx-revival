#pragma once

#include "../bbfx.h"
#include <OgreFrameListener.h>
#include <OgreMesh.h>
#include <OgreEntity.h>
#include <OgreHardwareBufferManager.h>

namespace bbfx {

class SoftwareVertexShader : public Ogre::FrameListener {
public:
    explicit SoftwareVertexShader(const string& meshName);
    virtual ~SoftwareVertexShader();

    bool frameStarted(const Ogre::FrameEvent& e) override;

    void enable();
    void disable();

    virtual void renderOneFrame(float dt) = 0;

protected:
    string mMeshName;
    Ogre::MeshPtr mOriginalMesh;
    Ogre::MeshPtr mClonedMesh;
};

} // namespace bbfx
