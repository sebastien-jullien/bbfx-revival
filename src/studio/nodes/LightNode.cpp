#include "LightNode.h"

namespace bbfx {

static int sLightCounter = 0;

LightNode::LightNode(const std::string& name, Ogre::SceneManager* scene)
    : AnimationNode(name), mScene(scene)
{
    ParamDef typeDef; typeDef.name = "light_type"; typeDef.label = "Type"; typeDef.type = ParamType::ENUM;
    typeDef.stringVal = "point"; typeDef.choices = {"point", "directional", "spot"};
    mSpec.addParam(typeDef);

    ParamDef diffDef; diffDef.name = "diffuse"; diffDef.label = "Diffuse"; diffDef.type = ParamType::COLOR;
    diffDef.colorVal[0] = 1; diffDef.colorVal[1] = 1; diffDef.colorVal[2] = 1;
    mSpec.addParam(diffDef);

    ParamDef powDef; powDef.name = "power"; powDef.label = "Power"; powDef.type = ParamType::FLOAT;
    powDef.floatVal = 1.0f; powDef.minVal = 0; powDef.maxVal = 10;
    mSpec.addParam(powDef);

    setParamSpec(&mSpec);

    addInput(new AnimationPort("power", 1.0f));
    addInput(new AnimationPort("position.x", 0.0f));
    addInput(new AnimationPort("position.y", 10.0f));
    addInput(new AnimationPort("position.z", 0.0f));
    addInput(new AnimationPort("diffuse.r", 1.0f));
    addInput(new AnimationPort("diffuse.g", 1.0f));
    addInput(new AnimationPort("diffuse.b", 1.0f));

    if (mScene) {
        std::string id = std::to_string(++sLightCounter);
        mLight = mScene->createLight("light_" + id);
        mLight->setType(Ogre::Light::LT_POINT);
        mLight->setDiffuseColour(1, 1, 1);
        mSceneNode = mScene->getRootSceneNode()->createChildSceneNode("lightsn_" + id);
        mSceneNode->attachObject(mLight);
        mSceneNode->setPosition(0, 10, 0);
    }
}

LightNode::~LightNode() { cleanup(); }

void LightNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (mLight) mLight->setVisible(en);
}

void LightNode::update() {
    if (!mLight || !mSceneNode) return;
    auto& in = getInputs();
    mSceneNode->setPosition(in.at("position.x")->getValue(),
                            in.at("position.y")->getValue(),
                            in.at("position.z")->getValue());
    // Read diffuse from ParamSpec COLOR (set by Inspector color picker)
    auto* diffParam = mSpec.getParam("diffuse");
    if (diffParam && diffParam->type == ParamType::COLOR) {
        mLight->setDiffuseColour(diffParam->colorVal[0], diffParam->colorVal[1], diffParam->colorVal[2]);
        // Sync ports with ParamSpec color values
        in.at("diffuse.r")->setValue(diffParam->colorVal[0]);
        in.at("diffuse.g")->setValue(diffParam->colorVal[1]);
        in.at("diffuse.b")->setValue(diffParam->colorVal[2]);
    } else {
        mLight->setDiffuseColour(in.at("diffuse.r")->getValue(),
                                 in.at("diffuse.g")->getValue(),
                                 in.at("diffuse.b")->getValue());
    }
    mLight->setPowerScale(in.at("power")->getValue());

    // Read light type from ParamSpec ENUM
    auto* typeParam = mSpec.getParam("light_type");
    if (typeParam) {
        Ogre::Light::LightTypes lt = Ogre::Light::LT_POINT;
        if (typeParam->stringVal == "directional") lt = Ogre::Light::LT_DIRECTIONAL;
        else if (typeParam->stringVal == "spot") lt = Ogre::Light::LT_SPOTLIGHT;
        if (mLight->getType() != lt) mLight->setType(lt);
    }

    fireUpdate();
}

void LightNode::cleanup() {
    if (mScene && mSceneNode) {
        if (mLight) mSceneNode->detachObject(mLight);
        mScene->destroySceneNode(mSceneNode);
        if (mLight) mScene->destroyLight(mLight);
        mSceneNode = nullptr; mLight = nullptr;
    }
}

} // namespace bbfx
