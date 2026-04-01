#include "MapperNode.h"
#include <cmath>
#include <algorithm>
namespace bbfx {

MapperNode::MapperNode(const std::string& name) : AnimationNode(name) {
    ParamDef curveDef; curveDef.name = "curve"; curveDef.label = "Curve"; curveDef.type = ParamType::ENUM;
    curveDef.stringVal = "linear"; curveDef.choices = {"linear","smooth","exponential","logarithmic"};
    mSpec.addParam(curveDef);
    ParamDef clampDef; clampDef.name = "clamp"; clampDef.label = "Clamp"; clampDef.type = ParamType::BOOL; clampDef.boolVal = true;
    mSpec.addParam(clampDef);
    setParamSpec(&mSpec);

    addInput(new AnimationPort("in", 0.0f));
    addInput(new AnimationPort("in_min", 0.0f));
    addInput(new AnimationPort("in_max", 1.0f));
    addInput(new AnimationPort("out_min", 0.0f));
    addInput(new AnimationPort("out_max", 1.0f));
    addInput(new AnimationPort("curve", 0.0f)); // 0=linear, 1=smooth, 2=exp, 3=log
    addOutput(new AnimationPort("out", 0.0f));
}

void MapperNode::update() {
    auto& in = getInputs();
    float val = in.at("in")->getValue();
    float inMin = in.at("in_min")->getValue();
    float inMax = in.at("in_max")->getValue();
    float outMin = in.at("out_min")->getValue();
    float outMax = in.at("out_max")->getValue();
    int curve = static_cast<int>(in.at("curve")->getValue());

    float range = inMax - inMin;
    float t = (range != 0.0f) ? (val - inMin) / range : 0.0f;
    if (mSpec.getParam("clamp")->boolVal) t = std::clamp(t, 0.0f, 1.0f);

    switch (curve) {
        case 1: t = t * t * (3.0f - 2.0f * t); break; // smooth (smoothstep)
        case 2: t = t * t; break; // exponential
        case 3: t = std::sqrt(t); break; // logarithmic
        default: break; // linear
    }
    getOutputs().at("out")->setValue(outMin + t * (outMax - outMin));
    fireUpdate();
}
} // namespace bbfx
