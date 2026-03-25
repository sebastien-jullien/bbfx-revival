#include "SoftwareVertexShader.h"
#include <OgreRoot.h>

namespace bbfx {

SoftwareVertexShader::SoftwareVertexShader(const string& meshName)
    : mMeshName(meshName) {}

SoftwareVertexShader::~SoftwareVertexShader() {
    disable();
}

bool SoftwareVertexShader::frameStarted(const Ogre::FrameEvent& e) {
    renderOneFrame(e.timeSinceLastFrame);
    return true;
}

void SoftwareVertexShader::enable() {
    Ogre::Root::getSingleton().addFrameListener(this);
}

void SoftwareVertexShader::disable() {
    Ogre::Root::getSingletonPtr()->removeFrameListener(this);
}

} // namespace bbfx
