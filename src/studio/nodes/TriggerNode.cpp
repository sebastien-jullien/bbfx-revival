#include "TriggerNode.h"
namespace bbfx {

TriggerNode::TriggerNode(const std::string& name) : AnimationNode(name) {
    ParamDef modeDef; modeDef.name = "mode"; modeDef.label = "Mode"; modeDef.type = ParamType::ENUM;
    modeDef.stringVal = "rising"; modeDef.choices = {"rising","falling","both","above","below"};
    mSpec.addParam(modeDef);
    ParamDef holdDef; holdDef.name = "hold_time"; holdDef.label = "Hold"; holdDef.type = ParamType::FLOAT;
    holdDef.floatVal = 0.1f; holdDef.minVal = 0; holdDef.maxVal = 5;
    mSpec.addParam(holdDef);
    setParamSpec(&mSpec);

    addInput(new AnimationPort("in", 0.0f));
    addInput(new AnimationPort("threshold", 0.5f));
    addInput(new AnimationPort("dt", 0.016f));
    addInput(new AnimationPort("mode", 0.0f)); // 0=rising, 1=falling, 2=both, 3=above, 4=below
    addOutput(new AnimationPort("trigger", 0.0f));
    addOutput(new AnimationPort("gate", 0.0f));
}

void TriggerNode::update() {
    auto& in = getInputs();
    float val = in.at("in")->getValue();
    float thresh = in.at("threshold")->getValue();
    float dt = in.at("dt")->getValue();
    int mode = static_cast<int>(in.at("mode")->getValue());

    bool above = val >= thresh;
    bool prevAbove = mPrevValue >= thresh;
    bool triggered = false;

    switch (mode) {
        case 0: triggered = above && !prevAbove; break;  // rising
        case 1: triggered = !above && prevAbove; break;  // falling
        case 2: triggered = above != prevAbove; break;   // both
        case 3: triggered = above; break;                // above (continuous)
        case 4: triggered = !above; break;               // below (continuous)
    }

    if (triggered) mHoldTimer = mSpec.getParam("hold_time")->floatVal;
    else if (mHoldTimer > 0) mHoldTimer -= dt;

    getOutputs().at("trigger")->setValue(mHoldTimer > 0 ? 1.0f : 0.0f);
    getOutputs().at("gate")->setValue(above ? 1.0f : 0.0f);
    mPrevValue = val;
    fireUpdate();
}
} // namespace bbfx
