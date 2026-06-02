#pragma once

#include "../core/AnimationNode.h"
#include "../core/ParamSpec.h"

namespace bbfx {

/// v3.5 Lot K — DAG node exposing the state of a connected gamepad.
///
/// All outputs are normalised floats (0..1 or -1..1) so they plug directly
/// into the rest of the animation graph without further scaling.
///
/// Outputs:
///   leftStickX / leftStickY    : -1..1
///   rightStickX / rightStickY  : -1..1
///   leftTrigger / rightTrigger : 0..1
///   buttonA..buttonBack        : 0 or 1 (per SDL gamepad button)
///   gyroX / gyroY / gyroZ      : deg/s (Kalman-filtered, offset-subtracted)
///   accelX / accelY / accelZ   : m/s²
///   touchpad0X / touchpad0Y    : 0..1 (0 when no finger)
///   touchpad0Down              : 0 or 1
///   touchpad1X / touchpad1Y    : 0..1
///   touchpad1Down              : 0 or 1
///   batteryPercent             : 0..1 (-1/100 when unknown)
class GamepadNode : public AnimationNode {
public:
    explicit GamepadNode(const std::string& name);
    void update() override;
    std::string getTypeName() const override { return "GamepadNode"; }

private:
    ParamSpec mSpec;   // single param: gamepad_index (INT)
    int       mIndex = 0;
    bool      mDbgFirstUpdate = true;   // v3.5.2 Lot AU.9 — one-time diagnostic log
    bool      mDbgWasActive   = false;  // edge-detect "anything pressed" for the diag log
};

} // namespace bbfx
