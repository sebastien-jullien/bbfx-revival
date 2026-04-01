#include "CompositorNode.h"
#include <OgreTexture.h>
#include <OgreCompositorManager.h>
#include <OgreViewport.h>
namespace bbfx {

CompositorNode::CompositorNode(const std::string& name, Ogre::Viewport* viewport)
    : AnimationNode(name), mViewport(viewport) {
    ParamDef compDef; compDef.name = "compositor"; compDef.label = "Compositor"; compDef.type = ParamType::COMPOSITOR;
    compDef.stringVal = "Bloom";
    mSpec.addParam(compDef);
    ParamDef enDef; enDef.name = "enabled"; enDef.label = "Enabled"; enDef.type = ParamType::BOOL; enDef.boolVal = true;
    mSpec.addParam(enDef);
    setParamSpec(&mSpec);

    addInput(new AnimationPort("enabled", 1.0f));
    mCompositorName = "";
}
CompositorNode::~CompositorNode() { cleanup(); }

void CompositorNode::update() {
    // Read compositor name from ParamSpec (for display in Inspector)
    auto* p = mSpec.getParam("compositor");
    if (p && !p->stringVal.empty()) mCompositorName = p->stringVal;

    // NOTE: Compositors are NOT activated on the Studio RenderTexture viewport.
    // OGRE compositors corrupt the FBO state when used on off-screen RenderTextures
    // shared with ImGui. The compositor name is stored for use in Performance Mode
    // and Export, where the scene renders to a RenderWindow viewport directly.
    // The node exists as a visual placeholder in the node editor showing which
    // compositor is configured.
    fireUpdate();
}

void CompositorNode::cleanup() {
    // Nothing to clean up — compositor is never activated on the Studio viewport
    mViewport = nullptr;
}
} // namespace bbfx
