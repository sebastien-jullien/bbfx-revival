#include "InputManager.h"

namespace bbfx {

InputManager* InputManager::sInstance = nullptr;

InputManager::InputManager() {
    assert(!sInstance);
    sInstance = this;
}

InputManager* InputManager::instance() {
    return sInstance;
}

void InputManager::capture() {
    mKeyboard.update();
    mMouse.update();
}

void InputManager::handleSDLEvent(const SDL_Event& evt) {
    mKeyboard.handleEvent(evt);
    mJoystick.handleEvent(evt);
}

KeyboardManager& InputManager::getKeyboard() { return mKeyboard; }
MouseManager& InputManager::getMouse() { return mMouse; }
JoystickManager& InputManager::getJoystick() { return mJoystick; }

} // namespace bbfx
