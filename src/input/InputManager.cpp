#include "InputManager.h"

namespace bbfx {

InputManager* InputManager::sInstance = nullptr;

InputManager::InputManager() {
    assert(!sInstance);
    sInstance = this;
    mJoystick.init();
}

InputManager* InputManager::instance() {
    return sInstance;
}

void InputManager::capture() {
    mKeyboard.update();
    mMouse.update();
    // v3.5 Lot J — advance rumble timers + poll gyro/accel sensors
    // exactly once per frame.
    mJoystick.update();
}

void InputManager::handleSDLEvent(const SDL_Event& evt) {
    mKeyboard.handleEvent(evt);
    mJoystick.handleEvent(evt);
}

KeyboardManager& InputManager::getKeyboard() { return mKeyboard; }
MouseManager& InputManager::getMouse() { return mMouse; }
JoystickManager& InputManager::getJoystick() { return mJoystick; }

} // namespace bbfx
