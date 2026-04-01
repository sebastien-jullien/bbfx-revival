#include "BeatTriggerNode.h"
#include <cmath>
namespace bbfx {

BeatTriggerNode::BeatTriggerNode(const std::string& name) : AnimationNode(name) {
    ParamDef subDef; subDef.name = "subdivision"; subDef.label = "Subdivision"; subDef.type = ParamType::ENUM;
    subDef.stringVal = "beat"; subDef.choices = {"beat","half","quarter","eighth","bar","cycle"};
    mSpec.addParam(subDef);
    ParamDef atkDef; atkDef.name = "attack"; atkDef.label = "Attack"; atkDef.type = ParamType::FLOAT;
    atkDef.floatVal = 0.02f; atkDef.minVal = 0.001f; atkDef.maxVal = 0.5f;
    mSpec.addParam(atkDef);
    ParamDef decDef; decDef.name = "decay"; decDef.label = "Decay"; decDef.type = ParamType::FLOAT;
    decDef.floatVal = 0.15f; decDef.minVal = 0.01f; decDef.maxVal = 2.0f;
    mSpec.addParam(decDef);
    ParamDef intDef; intDef.name = "intensity"; intDef.label = "Intensity"; intDef.type = ParamType::FLOAT;
    intDef.floatVal = 1.0f; intDef.minVal = 0; intDef.maxVal = 5;
    mSpec.addParam(intDef);
    setParamSpec(&mSpec);

    addInput(new AnimationPort("beat", 0.0f));
    addInput(new AnimationPort("beatFrac", 0.0f));
    addInput(new AnimationPort("dt", 0.016f));
    addOutput(new AnimationPort("trigger", 0.0f));
    addOutput(new AnimationPort("envelope", 0.0f));
    addOutput(new AnimationPort("phase", 0.0f));
}

void BeatTriggerNode::update() {
    auto& in = getInputs();
    auto& out = getOutputs();
    float beat = in.at("beat")->getValue();
    float beatFrac = in.at("beatFrac")->getValue();
    float dt = in.at("dt")->getValue();

    // Detect beat transitions
    float currentBeatFloor = std::floor(beat);
    bool triggered = (currentBeatFloor > mLastBeat && mLastBeat >= 0.0f);
    mLastBeat = currentBeatFloor;

    // Envelope: attack/decay
    float attack = mSpec.getParam("attack")->floatVal;
    float decay = mSpec.getParam("decay")->floatVal;
    float intensity = mSpec.getParam("intensity")->floatVal;

    if (triggered) {
        mEnvelope = intensity; // Jump to max on trigger
    } else if (mEnvelope > 0.0f) {
        mEnvelope -= dt / decay;
        if (mEnvelope < 0.0f) mEnvelope = 0.0f;
    }

    out.at("trigger")->setValue(triggered ? 1.0f : 0.0f);
    out.at("envelope")->setValue(mEnvelope);
    out.at("phase")->setValue(beatFrac);
    fireUpdate();
}

} // namespace bbfx
