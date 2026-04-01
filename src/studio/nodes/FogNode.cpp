#include "FogNode.h"
namespace bbfx {

FogNode::FogNode(const std::string& name, Ogre::SceneManager* scene)
    : AnimationNode(name), mScene(scene) {
    ParamDef typeDef; typeDef.name = "fog_type"; typeDef.label = "Type"; typeDef.type = ParamType::ENUM;
    typeDef.stringVal = "none"; typeDef.choices = {"none", "linear", "exp", "exp2"};
    mSpec.addParam(typeDef);
    ParamDef colDef; colDef.name = "color"; colDef.label = "Color"; colDef.type = ParamType::COLOR;
    colDef.colorVal[0] = 0.5f; colDef.colorVal[1] = 0.5f; colDef.colorVal[2] = 0.5f;
    mSpec.addParam(colDef);
    ParamDef densDef; densDef.name = "density"; densDef.label = "Density"; densDef.type = ParamType::FLOAT;
    densDef.floatVal = 0.001f; densDef.minVal = 0; densDef.maxVal = 0.1f; densDef.stepVal = 0.001f;
    mSpec.addParam(densDef);
    ParamDef startDef; startDef.name = "start"; startDef.label = "Start"; startDef.type = ParamType::FLOAT;
    startDef.floatVal = 50; startDef.minVal = 0; startDef.maxVal = 1000;
    mSpec.addParam(startDef);
    ParamDef endDef; endDef.name = "end"; endDef.label = "End"; endDef.type = ParamType::FLOAT;
    endDef.floatVal = 500; endDef.minVal = 0; endDef.maxVal = 5000;
    mSpec.addParam(endDef);
    setParamSpec(&mSpec);

    addInput(new AnimationPort("density", 0.001f));
    addInput(new AnimationPort("start", 50.0f));
    addInput(new AnimationPort("end", 500.0f));
}

void FogNode::update() {
    if (!mScene) return;
    auto& in = getInputs();
    auto* typeDef = mSpec.getParam("fog_type");
    auto* colDef = mSpec.getParam("color");
    if (!typeDef) return;

    Ogre::FogMode mode = Ogre::FOG_NONE;
    if (typeDef->stringVal == "linear") mode = Ogre::FOG_LINEAR;
    else if (typeDef->stringVal == "exp") mode = Ogre::FOG_EXP;
    else if (typeDef->stringVal == "exp2") mode = Ogre::FOG_EXP2;

    Ogre::ColourValue col(0.5f, 0.5f, 0.5f);
    if (colDef) col = Ogre::ColourValue(colDef->colorVal[0], colDef->colorVal[1], colDef->colorVal[2]);

    mScene->setFog(mode, col, in.at("density")->getValue(),
                   in.at("start")->getValue(), in.at("end")->getValue());
    fireUpdate();
}

void FogNode::cleanup() {
    if (mScene) {
        mScene->setFog(Ogre::FOG_NONE);
        mScene = nullptr;
    }
}
} // namespace bbfx
