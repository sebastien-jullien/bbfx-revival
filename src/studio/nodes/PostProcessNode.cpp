#include "PostProcessNode.h"
#include <iostream>

namespace bbfx {

PostProcessNode::PostProcessNode(const std::string& name, PostProcessStack* stack)
    : AnimationNode(name), mStack(stack)
{
    // Effect name in the stack is unique per node instance
    mEffectName = "pp_" + name;

    // ParamSpec: compositor selector + enabled + order
    ParamDef compDef;
    compDef.name = "compositor";
    compDef.label = "Effect";
    compDef.type = ParamType::COMPOSITOR;
    compDef.stringVal = "Vignette";
    mSpec.addParam(compDef);

    ParamDef enDef;
    enDef.name = "enabled";
    enDef.label = "Enabled";
    enDef.type = ParamType::BOOL;
    enDef.boolVal = true;
    mSpec.addParam(enDef);

    ParamDef orderDef;
    orderDef.name = "order";
    orderDef.label = "Order";
    orderDef.type = ParamType::INT;
    orderDef.intVal = 0;
    orderDef.minVal = -100.0f;
    orderDef.maxVal = 100.0f;
    mSpec.addParam(orderDef);

    setParamSpec(&mSpec);

    // v3.5.2 Sprint S8 Lot AT — `enabled` port provided by AnimationNode base.

    // Register the default effect in the stack
    if (mStack) {
        std::string matName = "BBFx/" + compDef.stringVal;
        if (mStack->addEffect(mEffectName, matName)) {
            mRegistered = true;
            mCurrentMaterial = matName;
        }
    }
}

PostProcessNode::~PostProcessNode() {
    cleanup();
}

void PostProcessNode::update() {
    if (!mStack) { fireUpdate(); return; }

    auto* compParam = mSpec.getParam("compositor");
    auto* enParam = mSpec.getParam("enabled");
    auto* orderParam = mSpec.getParam("order");

    // Read the compositor name from ParamSpec
    std::string newName = compParam ? compParam->stringVal : "Vignette";
    std::string newMat = "BBFx/" + newName;

    // If material changed, re-register effect
    if (newMat != mCurrentMaterial) {
        if (mRegistered) {
            mStack->removeEffect(mEffectName);
            mRegistered = false;
        }
        if (mStack->addEffect(mEffectName, newMat)) {
            mRegistered = true;
            mCurrentMaterial = newMat;
        }
    }

    // Sync enabled state (from port and param)
    bool nodeEnabled = isEnabled();
    bool paramEnabled = enParam ? enParam->boolVal : true;
    float portVal = 1.0f;
    auto& ins = getInputs();
    auto it = ins.find("enabled");
    if (it != ins.end()) portVal = it->second->getValue();
    bool effectEnabled = nodeEnabled && paramEnabled && (portVal > 0.5f);
    mStack->setEnabled(mEffectName, effectEnabled);

    // Sync order
    if (orderParam) {
        mStack->reorder(mEffectName, orderParam->intVal);
    }

    fireUpdate();
}

void PostProcessNode::cleanup() {
    if (mStack && mRegistered) {
        mStack->removeEffect(mEffectName);
        mRegistered = false;
    }
    mStack = nullptr;
}

void PostProcessNode::setEnabled(bool en) {
    AnimationNode::setEnabled(en);
    if (mStack) {
        mStack->setEnabled(mEffectName, en);
    }
}

} // namespace bbfx
