#include "GamepadNode.h"

#include "GamepadManager.h"
#include "InputManager.h"

#include <SDL3/SDL.h>

namespace bbfx {

GamepadNode::GamepadNode(const std::string& name) : AnimationNode(name) {
    ParamDef idx;
    idx.name = "gamepad_index";
    idx.label = "Gamepad Index";
    idx.type = ParamType::INT;
    idx.intVal = 0;
    idx.minVal = 0;
    idx.maxVal = 7;   // up to 8 gamepads simultaneously
    idx.stepVal = 1;
    mSpec.addParam(idx);
    setParamSpec(&mSpec);

    // Sticks + triggers + common buttons
    addOutput(new AnimationPort("leftStickX",    0.0f));
    addOutput(new AnimationPort("leftStickY",    0.0f));
    addOutput(new AnimationPort("rightStickX",   0.0f));
    addOutput(new AnimationPort("rightStickY",   0.0f));
    addOutput(new AnimationPort("leftTrigger",   0.0f));
    addOutput(new AnimationPort("rightTrigger",  0.0f));

    addOutput(new AnimationPort("buttonA",       0.0f));
    addOutput(new AnimationPort("buttonB",       0.0f));
    addOutput(new AnimationPort("buttonX",       0.0f));
    addOutput(new AnimationPort("buttonY",       0.0f));
    addOutput(new AnimationPort("leftBumper",    0.0f));
    addOutput(new AnimationPort("rightBumper",   0.0f));
    addOutput(new AnimationPort("leftStickBtn",  0.0f));
    addOutput(new AnimationPort("rightStickBtn", 0.0f));
    addOutput(new AnimationPort("start",         0.0f));
    addOutput(new AnimationPort("back",          0.0f));
    addOutput(new AnimationPort("dpadUp",        0.0f));
    addOutput(new AnimationPort("dpadDown",      0.0f));
    addOutput(new AnimationPort("dpadLeft",      0.0f));
    addOutput(new AnimationPort("dpadRight",     0.0f));

    // Sensors
    addOutput(new AnimationPort("gyroX",         0.0f));
    addOutput(new AnimationPort("gyroY",         0.0f));
    addOutput(new AnimationPort("gyroZ",         0.0f));
    addOutput(new AnimationPort("accelX",        0.0f));
    addOutput(new AnimationPort("accelY",        0.0f));
    addOutput(new AnimationPort("accelZ",        0.0f));

    // Touchpad (0..1 normalised, 0 when no finger)
    addOutput(new AnimationPort("touchpad0X",    0.0f));
    addOutput(new AnimationPort("touchpad0Y",    0.0f));
    addOutput(new AnimationPort("touchpad0Down", 0.0f));
    addOutput(new AnimationPort("touchpad1X",    0.0f));
    addOutput(new AnimationPort("touchpad1Y",    0.0f));
    addOutput(new AnimationPort("touchpad1Down", 0.0f));

    // Battery percent normalised to 0..1 (-0.01 when unknown so downstream
    // graphs can branch cleanly on negative).
    addOutput(new AnimationPort("batteryPercent", -0.01f));
}

void GamepadNode::update() {
    auto* im = InputManager::instance();
    if (!im) return;
    auto& mgr = im->getJoystick();

    // Resolve gamepad_index from ParamSpec (user-edited) on every tick —
    // cheap and lets the user switch gamepads live.
    if (auto* p = mSpec.getParam("gamepad_index")) {
        mIndex = p->intVal;
    }
    int idx = mIndex;
    if (idx < 0 || idx >= mgr.getCount()) {
        // No device — zero everything. (Keeps downstream graph idle.)
        for (auto& [name, port] : getOutputs()) port->setValue(0.0f);
        fireUpdate();
        return;
    }

    auto& out = getOutputs();
    auto set = [&](const char* k, float v) { out.at(k)->setValue(v); };

    set("leftStickX",    mgr.getAxisValue(idx, SDL_GAMEPAD_AXIS_LEFTX));
    set("leftStickY",    mgr.getAxisValue(idx, SDL_GAMEPAD_AXIS_LEFTY));
    set("rightStickX",   mgr.getAxisValue(idx, SDL_GAMEPAD_AXIS_RIGHTX));
    set("rightStickY",   mgr.getAxisValue(idx, SDL_GAMEPAD_AXIS_RIGHTY));
    set("leftTrigger",   mgr.getAxisValue(idx, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    set("rightTrigger",  mgr.getAxisValue(idx, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));

    auto button = [&](SDL_GamepadButton b) {
        return mgr.isButtonDown(idx, static_cast<int>(b)) ? 1.0f : 0.0f;
    };
    set("buttonA",       button(SDL_GAMEPAD_BUTTON_SOUTH));
    set("buttonB",       button(SDL_GAMEPAD_BUTTON_EAST));
    set("buttonX",       button(SDL_GAMEPAD_BUTTON_WEST));
    set("buttonY",       button(SDL_GAMEPAD_BUTTON_NORTH));
    set("leftBumper",    button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER));
    set("rightBumper",   button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER));
    set("leftStickBtn",  button(SDL_GAMEPAD_BUTTON_LEFT_STICK));
    set("rightStickBtn", button(SDL_GAMEPAD_BUTTON_RIGHT_STICK));
    set("start",         button(SDL_GAMEPAD_BUTTON_START));
    set("back",          button(SDL_GAMEPAD_BUTTON_BACK));
    set("dpadUp",        button(SDL_GAMEPAD_BUTTON_DPAD_UP));
    set("dpadDown",      button(SDL_GAMEPAD_BUTTON_DPAD_DOWN));
    set("dpadLeft",      button(SDL_GAMEPAD_BUTTON_DPAD_LEFT));
    set("dpadRight",     button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT));

    float gx, gy, gz;
    mgr.getGyro(idx, gx, gy, gz);
    set("gyroX", gx); set("gyroY", gy); set("gyroZ", gz);

    float ax, ay, az;
    mgr.getAccel(idx, ax, ay, az);
    set("accelX", ax); set("accelY", ay); set("accelZ", az);

    GamepadManager::TouchFinger fingers[2];
    mgr.getTouchpadFingers(idx, fingers);
    set("touchpad0X",    fingers[0].x);
    set("touchpad0Y",    fingers[0].y);
    set("touchpad0Down", fingers[0].down ? 1.0f : 0.0f);
    set("touchpad1X",    fingers[1].x);
    set("touchpad1Y",    fingers[1].y);
    set("touchpad1Down", fingers[1].down ? 1.0f : 0.0f);

    int pct = mgr.getBatteryPercent(idx);
    set("batteryPercent", pct < 0 ? -0.01f : (pct / 100.0f));

    fireUpdate();
}

} // namespace bbfx
