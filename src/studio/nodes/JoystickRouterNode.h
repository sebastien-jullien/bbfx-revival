#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <string>

namespace bbfx {

/// JoystickRouterNode — converts a GamepadNode's raw button + axis output into
/// higher-level signals (gated value, edge trigger, toggle). Reproduces the
/// BBFx 2006 `Joystick.rules[button] = function | table` pattern (cf.
/// joystick.lua:97-115).
///
/// Modes:
///   - HoldGate     : output = axis_value * (button_held ? 1 : 0)  (scrollbutton)
///   - PressTrigger : output = 1 on button rising edge, 0 otherwise (sweepbutton)
///   - Toggle       : output flips 0 <-> 1 each press (sticky toggle)
///
/// Index mapping (button_index 0..13, axis_index 0..5):
///   button 0=A, 1=B, 2=X, 3=Y, 4=LB, 5=RB, 6=back, 7=start,
///          8=LS, 9=RS, 10=dpadUp, 11=dpadDown, 12=dpadLeft, 13=dpadRight
///   axis   0=leftStickX, 1=leftStickY, 2=rightStickX, 3=rightStickY,
///          4=leftTrigger, 5=rightTrigger
class JoystickRouterNode : public AnimationNode {
public:
    enum class RouterMode { HoldGate, PressTrigger, Toggle };

    explicit JoystickRouterNode(const std::string& name);
    ~JoystickRouterNode() override = default;

    void update() override;
    void onFrameAdvance() override { mFrameArmed = true; }   // arm the once-per-frame edge eval
    std::string getTypeName() const override { return "JoystickRouterNode"; }

    RouterMode getMode() const { return mMode; }
    int getButtonIndex() const { return mButtonIndex; }
    int getAxisIndex()   const { return mAxisIndex; }
    float getLastButtonValue() const { return mLastButton; }
    float getLastAxisValue()   const { return mLastAxis; }
    float getGatedValue() const { return mLastGated; }
    float getTrigger()    const { return mLastTrigger; }
    float getToggled()    const { return mLastToggled; }

    static const char* portNameForButton(int index);
    static const char* portNameForAxis(int index);

private:
    ParamSpec mSpec;
    int  mButtonIndex = 0;
    int  mAxisIndex   = 0;
    RouterMode mMode = RouterMode::HoldGate;

    bool  mFrameArmed = false;        // set by onFrameAdvance(); edges are evaluated only when true
    bool  mPrevButtonState = false;
    bool  mToggleState = false;
    float mLastButton  = 0.0f;
    float mLastAxis    = 0.0f;
    float mLastGated   = 0.0f;
    float mLastTrigger = 0.0f;
    float mLastToggled = 0.0f;

    AnimationNode* resolveGamepad();
};

} // namespace bbfx
