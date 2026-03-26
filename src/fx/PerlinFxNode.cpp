#include "PerlinFxNode.h"

namespace bbfx {

PerlinFxNode::PerlinFxNode(const string& meshName, const string& cloneName)
    : AnimationNode("PerlinFxNode")
    , mShader(std::make_unique<PerlinVertexShader>(meshName, cloneName))
{
    addInput(new AnimationPort("dt", 0.016f));
    addInput(new AnimationPort("displacement", 0.1f));
    addInput(new AnimationPort("density", 50.0f));
    addInput(new AnimationPort("timeDensity", 5.0f));
    addInput(new AnimationPort("enable", 1.0f));
    addOutput(new AnimationPort("mesh_dirty", 0.0f));
}

PerlinFxNode::~PerlinFxNode() = default;

void PerlinFxNode::update() {
    auto& inputs = getInputs();
    auto& outputs = getOutputs();

    float enableVal = inputs.at("enable")->getValue();
    if (enableVal >= 0.5f) {
        mShader->setDisplacement(inputs.at("displacement")->getValue());
        mShader->setDensity(inputs.at("density")->getValue());
        mShader->setTimeDensity(inputs.at("timeDensity")->getValue());
        float dt = inputs.at("dt")->getValue();
        mShader->renderOneFrame(dt);
        outputs.at("mesh_dirty")->setValue(1.0f);
    } else {
        outputs.at("mesh_dirty")->setValue(0.0f);
    }
    fireUpdate();
}

void PerlinFxNode::enable() {
    mShader->enable();
}

void PerlinFxNode::disable() {
    mShader->disable();
}

} // namespace bbfx
