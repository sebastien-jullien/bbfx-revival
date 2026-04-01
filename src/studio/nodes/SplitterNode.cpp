#include "SplitterNode.h"
namespace bbfx {

SplitterNode::SplitterNode(const std::string& name) : AnimationNode(name) {
    ParamDef numDef; numDef.name = "outputs"; numDef.label = "Outputs"; numDef.type = ParamType::INT;
    numDef.intVal = 4; numDef.minVal = 2; numDef.maxVal = 8;
    mSpec.addParam(numDef);
    setParamSpec(&mSpec);

    addInput(new AnimationPort("in", 0.0f));
    for (int i = 1; i <= mNumOutputs; i++)
        addOutput(new AnimationPort("out_" + std::to_string(i), 0.0f));
}

void SplitterNode::update() {
    float val = getInputs().at("in")->getValue();
    for (auto& [name, port] : getOutputs())
        port->setValue(val);
    fireUpdate();
}
} // namespace bbfx
