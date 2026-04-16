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
    if (!mScene) { fireUpdate(); return; }

    auto* p = mSpec.getParam("bg_color");
    if (p) {
        Ogre::ColourValue col(p->colorVal[0], p->colorVal[1], p->colorVal[2]);
        mScene->setAmbientLight(col);
    }

    // Apply skybox rotation from input port.
    auto& ins = getInputs();
    auto rotIt = ins.find("rotation");
    if (rotIt != ins.end()) {
        float rot = rotIt->second->getValue();
        auto* skyNode = mScene->getSkyBoxNode();
        if (skyNode) skyNode->setOrientation(Ogre::Quaternion(Ogre::Degree(rot), Ogre::Vector3::UNIT_Y));
    }

    fireUpdate();
}
} // namespace bbfx
