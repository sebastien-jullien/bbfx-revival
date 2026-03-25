#pragma once

#include "KeyboardManager.h"
#include "MouseManager.h"
#include "JoystickManager.h"

namespace bbfx {

class InputManager {
public:
    InputManager();
    static InputManager* instance();

    void capture();
    void handleSDLEvent(const SDL_Event& evt);

    KeyboardManager& getKeyboard();
    MouseManager& getMouse();
    JoystickManager& getJoystick();

private:
    static InputManager* sInstance;
    KeyboardManager mKeyboard;
    MouseManager mMouse;
    JoystickManager mJoystick;
};

} // namespace bbfx
