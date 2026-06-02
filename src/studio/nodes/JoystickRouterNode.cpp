#include "JoystickRouterNode.h"
#include "../../core/Animator.h"
#include "../../core/DebugLog.h"
#include "../../input/GamepadNode.h"
#include <iostream>

namespace bbfx {

JoystickRouterNode::JoystickRouterNode(const std::string& name)
    : AnimationNode(name)
{
    ParamDef btnDef;
    btnDef.name = "button_index"; btnDef.label = "Button";
    btnDef.type = ParamType::INT;
    btnDef.intVal = 0; btnDef.minVal = 0; btnDef.maxVal = 13;
    mSpec.addParam(btnDef);

    ParamDef axDef;
    axDef.name = "axis_index"; axDef.label = "Axis";
    axDef.type = ParamType::INT;
    axDef.intVal = 0; axDef.minVal = 0; axDef.maxVal = 5;
    mSpec.addParam(axDef);

    ParamDef modeDef;
    modeDef.name = "mode"; modeDef.label = "Mode"; modeDef.type = ParamType::ENUM;
    modeDef.stringVal = "hold_gate";
    // Lot AV — `scroll_gate` est un alias sémantique de `hold_gate` (axis × button_held).
    // Le 2006 utilisait le terme « scrollbutton » dans textureset.lua → on l'expose
    // explicitement pour clarté quand on monte une chaîne TextureSet/scrollbutton.
    modeDef.choices = {"hold_gate", "press_trigger", "toggle", "scroll_gate"};
    mSpec.addParam(modeDef);

    setParamSpec(&mSpec);

    addInput(new AnimationPort("gamepad", 0.0f));   // entity-link
    addInput(new AnimationPort("button",  0.0f));   // direct override (bypasses gamepad)
    addInput(new AnimationPort("axis",    0.0f));   // direct override

    addOutput(new AnimationPort("gated_value", 0.0f));
    addOutput(new AnimationPort("trigger",     0.0f));
    addOutput(new AnimationPort("toggled",     0.0f));

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("gamepad",     "Entity-link to a GamepadNode. Pulls button/axis state via portNameForButton/Axis.");
    setPortTooltip("button",      "Direct override of the gating button (0..1). Bypasses gamepad when set.");
    setPortTooltip("axis",        "Direct override of the routed axis value (-1..1).");
    setPortTooltip("gated_value", "In hold_gate (= scroll_gate alias 2006) mode: axis value while button held. In trigger modes: pulse on edge.");
    setPortTooltip("trigger",     "Pulses 1.0 for one frame on press_trigger rising edge (otherwise 0).");
    setPortTooltip("toggled",     "Toggled state (toggle mode): flips between 0 and 1 on each press.");
}

const char* JoystickRouterNode::portNameForButton(int index) {
    switch (index) {
    case 0:  return "buttonA";
    case 1:  return "buttonB";
    case 2:  return "buttonX";
    case 3:  return "buttonY";
    case 4:  return "leftBumper";
    case 5:  return "rightBumper";
    case 6:  return "back";
    case 7:  return "start";
    case 8:  return "leftStickBtn";
    case 9:  return "rightStickBtn";
    case 10: return "dpadUp";
    case 11: return "dpadDown";
    case 12: return "dpadLeft";
    case 13: return "dpadRight";
    default: return "buttonA";
    }
}

const char* JoystickRouterNode::portNameForAxis(int index) {
    switch (index) {
    case 0: return "leftStickX";
    case 1: return "leftStickY";
    case 2: return "rightStickX";
    case 3: return "rightStickY";
    case 4: return "leftTrigger";
    case 5: return "rightTrigger";
    default: return "leftStickX";
    }
}

AnimationNode* JoystickRouterNode::resolveGamepad() {
    auto* animator = Animator::instance();
    if (!animator) return nullptr;
    auto& in = getInputs();
    auto it = in.find("gamepad");
    if (it == in.end()) return nullptr;
    auto sources = animator->getSourceNodes(it->second);
    for (auto* src : sources) {
        if (auto* gp = dynamic_cast<GamepadNode*>(src)) return gp;
    }
    return nullptr;
}

void JoystickRouterNode::update() {
    if (!mEnabled) return;

    // Sync ParamSpec -> internal state
    auto* btnParam = mSpec.getParam("button_index");
    if (btnParam) mButtonIndex = btnParam->intVal;
    auto* axParam = mSpec.getParam("axis_index");
    if (axParam) mAxisIndex = axParam->intVal;
    auto* modeParam = mSpec.getParam("mode");
    if (modeParam) {
        if      (modeParam->stringVal == "press_trigger") mMode = RouterMode::PressTrigger;
        else if (modeParam->stringVal == "toggle")        mMode = RouterMode::Toggle;
        // Lot AV — `scroll_gate` est un alias 2006 du mode HoldGate (équivalent
        // fonctionnel : axis × button_held). Plus parlant dans une chaîne TextureSet.
        else if (modeParam->stringVal == "scroll_gate")   mMode = RouterMode::HoldGate;
        else                                              mMode = RouterMode::HoldGate;
    }

    // Read raw values: prefer entity-link gamepad if connected, else direct overrides.
    float btnVal = 0.0f;
    float axVal  = 0.0f;
    auto* gp = resolveGamepad();
    if (gp) {
        const auto& outs = gp->getOutputs();
        const std::string btnName = portNameForButton(mButtonIndex);
        const std::string axName  = portNameForAxis(mAxisIndex);
        auto bit = outs.find(btnName);
        if (bit != outs.end()) btnVal = bit->second->getValue();
        auto ait = outs.find(axName);
        if (ait != outs.end()) axVal  = ait->second->getValue();
    } else {
        auto& in = getInputs();
        auto bit = in.find("button");
        if (bit != in.end()) btnVal = bit->second->getValue();
        auto ait = in.find("axis");
        if (ait != in.end()) axVal  = ait->second->getValue();
    }
    mLastButton = btnVal;
    mLastAxis   = axVal;

    const bool buttonDown = (btnVal > 0.5f);

    // Edge detection runs at most ONCE per frame (mFrameArmed is set by onFrameAdvance()
    // from tick()). update() is re-entered several times within a single frame by the
    // DAG propagation cascade — if mPrevButtonState were updated on every call, the
    // rising edge would be consumed mid-frame and the one-frame `trigger` pulse would be
    // cleared before it has been propagated to downstream consumers (TextureCycle.next,
    // TheoraClip.play, …). Between frames the cached mLast* outputs simply persist, so
    // the pulse survives re-entrant propagation calls until it has reached its consumers.
    if (mFrameArmed) {
        mFrameArmed = false;
        const bool risingEdge = (buttonDown && !mPrevButtonState);
        // I-2053 diag (Lot AV.3) — trace 1 ligne par rising edge live, pour identifier
        // les problèmes de mapping gamepad (ex. Thrustmaster Dual Analog 4 où certains
        // contrôleurs ne renvoient pas SDL_GAMEPAD_BUTTON_SOUTH=0 sur le bouton attendu).
        if (risingEdge) {
            BBFX_DLOG("[JR " << getName() << "] rising edge button_index=" << mButtonIndex
                      << " btnVal=" << btnVal << " mode="
                      << (mMode == RouterMode::PressTrigger ? "press_trigger"
                          : mMode == RouterMode::Toggle ? "toggle" : "hold_gate"));
        }
        mPrevButtonState = buttonDown;
        mLastTrigger = 0.0f;
        switch (mMode) {
        case RouterMode::PressTrigger:
            mLastTrigger = risingEdge ? 1.0f : 0.0f;
            break;
        case RouterMode::Toggle:
            if (risingEdge) mToggleState = !mToggleState;
            break;
        case RouterMode::HoldGate:
            break;   // gated value is level-based, recomputed below every call
        }
        mLastToggled = mToggleState ? 1.0f : 0.0f;
    }

    // Level outputs — safe (and cheap) to recompute on every call.
    mLastGated = (mMode == RouterMode::HoldGate && buttonDown) ? axVal : 0.0f;

    auto& out = getOutputs();
    out.at("gated_value")->setValue(mLastGated);
    out.at("trigger"    )->setValue(mLastTrigger);
    out.at("toggled"    )->setValue(mLastToggled);

    fireUpdate();
}

} // namespace bbfx
