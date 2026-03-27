#pragma once

#include "../bbfx.h"
#include <SDL3/SDL.h>
#include <vector>

namespace bbfx {

class JoystickManager {
public:
    JoystickManager();
    ~JoystickManager();

    void init();
    void handleEvent(const SDL_Event& evt);

    float getAxisValue(int joystickId, int axis) const;
    bool isButtonDown(int joystickId, int button) const;
    int getCount() const;

private:
    struct GamepadEntry {
        SDL_JoystickID id;
        SDL_Gamepad*   gamepad;   // non-null if SDL gamepad mapping exists
        SDL_Joystick*  joystick;  // non-null if raw joystick fallback
    };
    std::vector<GamepadEntry> mGamepads;

    SDL_Gamepad* findGamepad(int joystickId) const;
};

} // namespace bbfx
