#include "MathNode.h"
#include <cmath>
#include <algorithm>

namespace bbfx {

MathNode::MathNode(const std::string& name) : AnimationNode(name) {
    ParamDef opDef;
    opDef.name = "operation"; opDef.label = "Operation"; opDef.type = ParamType::ENUM;
    opDef.stringVal = "add";
    opDef.choices = {"add","subtract","multiply","divide","modulo","power",
                     "min","max","abs","clamp","lerp","smoothstep","sin","cos","noise"};
    mSpec.addParam(opDef);
    setParamSpec(&mSpec);

    addInput(new AnimationPort("a", 0.0f));
    addInput(new AnimationPort("b", 0.0f));
    addInput(new AnimationPort("t", 0.0f));
    addInput(new AnimationPort("operation", 0.0f)); // 0=add, 1=sub, ...
    addOutput(new AnimationPort("out", 0.0f));
}

void MathNode::update() {
    auto& in = getInputs();
    float a = in.at("a")->getValue();
    float b = in.at("b")->getValue();
    float t = in.at("t")->getValue();
    int op = static_cast<int>(in.at("operation")->getValue());

    float result = 0.0f;
    switch (op) {
        case 0:  result = a + b; break;         // add
        case 1:  result = a - b; break;         // subtract
        case 2:  result = a * b; break;         // multiply
        case 3:  result = (b != 0.0f) ? a / b : 0.0f; break; // divide
        case 4:  result = (b != 0.0f) ? std::fmod(a, b) : 0.0f; break; // modulo
        case 5:  result = std::pow(a, b); break; // power
        case 6:  result = std::min(a, b); break; // min
        case 7:  result = std::max(a, b); break; // max
        case 8:  result = std::abs(a); break;   // abs
        case 9:  result = std::clamp(a, std::min(b, t), std::max(b, t)); break; // clamp(a, b, t)
        case 10: result = a + t * (b - a); break; // lerp(a, b, t)
        case 11: { float x = std::clamp(t, 0.0f, 1.0f); result = x * x * (3.0f - 2.0f * x); break; } // smoothstep
        case 12: result = std::sin(a); break;   // sin
        case 13: result = std::cos(a); break;   // cos
        default: result = a + b; break;
    }
    getOutputs().at("out")->setValue(result);
    fireUpdate();
}

} // namespace bbfx
