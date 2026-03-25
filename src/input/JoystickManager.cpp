#include "JoystickManager.h"

namespace bbfx {

JoystickManager::JoystickManager() = default;

JoystickManager::~JoystickManager() {
    for (auto& entry : mGamepads) {
        if (entry.gamepad) {
            SDL_CloseGamepad(entry.gamepad);
        }
    }
}

void JoystickManager::handleEvent(const SDL_Event& evt) {
    if (evt.type == SDL_EVENT_GAMEPAD_ADDED) {
        SDL_Gamepad* gp = SDL_OpenGamepad(evt.gdevice.which);
        if (gp) {
            mGamepads.push_back({evt.gdevice.which, gp});
        }
    } else if (evt.type == SDL_EVENT_GAMEPAD_REMOVED) {
        for (auto it = mGamepads.begin(); it != mGamepads.end(); ++it) {
            if (it->id == evt.gdevice.which) {
                SDL_CloseGamepad(it->gamepad);
                mGamepads.erase(it);
                break;
            }
        }
    }
}

float JoystickManager::getAxisValue(int joystickId, int axis) const {
    auto* gp = findGamepad(joystickId);
    if (gp) {
        Sint16 val = SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis));
        return static_cast<float>(val) / 32767.0f;
    }
    return 0.0f;
}

bool JoystickManager::isButtonDown(int joystickId, int button) const {
    auto* gp = findGamepad(joystickId);
    if (gp) {
        return SDL_GetGamepadButton(gp, static_cast<SDL_GamepadButton>(button));
    }
    return false;
}

int JoystickManager::getCount() const {
    return static_cast<int>(mGamepads.size());
}

SDL_Gamepad* JoystickManager::findGamepad(int joystickId) const {
    if (joystickId >= 0 && joystickId < static_cast<int>(mGamepads.size())) {
        return mGamepads[joystickId].gamepad;
    }
    return nullptr;
}

} // namespace bbfx
