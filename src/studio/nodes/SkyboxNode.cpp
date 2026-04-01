#include "SkyboxNode.h"
namespace bbfx {

SkyboxNode::SkyboxNode(const std::string& name, Ogre::SceneManager* scene)
    : AnimationNode(name), mScene(scene) {
    ParamDef typeDef; typeDef.name = "sky_type"; typeDef.label = "Type"; typeDef.type = ParamType::ENUM;
    typeDef.stringVal = "color"; typeDef.choices = {"skybox", "color", "gradient"};
    mSpec.addParam(typeDef);
    ParamDef bgDef; bgDef.name = "bg_color"; bgDef.label = "Background"; bgDef.type = ParamType::COLOR;
    bgDef.colorVal[0] = 0.05f; bgDef.colorVal[1] = 0.05f; bgDef.colorVal[2] = 0.1f;
    mSpec.addParam(bgDef);
    setParamSpec(&mSpec);
    addInput(new AnimationPort("rotation", 0.0f));
}

void SkyboxNode::update() {
    // Apply background color from ParamSpec
    if (mScene) {
        auto* p = mSpec.getParam("bg_color");
        if (p) {
            // Background color is set via viewport, not scene — this is a placeholder
            // Real implementation needs StudioEngine viewport access
        }
    }
    fireUpdate();
}
} // namespace bbfx
