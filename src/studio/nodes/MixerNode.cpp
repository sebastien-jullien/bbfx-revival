#include "MixerNode.h"
#include <algorithm>
namespace bbfx {

MixerNode::MixerNode(const std::string& name) : AnimationNode(name) {
    ParamDef modeDef; modeDef.name = "mode"; modeDef.label = "Mode"; modeDef.type = ParamType::ENUM;
    modeDef.stringVal = "average"; modeDef.choices = {"average","max","min","sum","weighted"};
    mSpec.addParam(modeDef);
    setParamSpec(&mSpec);

    for (int i = 1; i <= mNumInputs; i++)
        addInput(new AnimationPort("in_" + std::to_string(i), 0.0f));
    addInput(new AnimationPort("mode", 0.0f)); // 0=avg, 1=max, 2=min, 3=sum, 4=weighted
    addOutput(new AnimationPort("out", 0.0f));
}

void MixerNode::update() {
    auto& in = getInputs();
    int mode = static_cast<int>(in.at("mode")->getValue());
    float result = 0, maxV = -1e9f, minV = 1e9f, sum = 0;
    int count = 0;
    for (int i = 1; i <= mNumInputs; i++) {
        auto it = in.find("in_" + std::to_string(i));
        if (it == in.end()) continue;
        float v = it->second->getValue();
        sum += v; count++;
        maxV = std::max(maxV, v);
        minV = std::min(minV, v);
    }
    switch (mode) {
        case 0: result = count > 0 ? sum / count : 0; break; // average
        case 1: result = maxV; break; // max
        case 2: result = minV; break; // min
        case 3: result = sum; break;  // sum
        default: result = count > 0 ? sum / count : 0; break;
    }
    getOutputs().at("out")->setValue(result);
    fireUpdate();
}
} // namespace bbfx
