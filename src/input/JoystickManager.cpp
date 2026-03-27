#include "JoystickManager.h"

namespace bbfx {

JoystickManager::JoystickManager() = default;

JoystickManager::~JoystickManager() {
    for (auto& entry : mGamepads) {
        if (entry.gamepad)  SDL_CloseGamepad(entry.gamepad);
        if (entry.joystick) SDL_CloseJoystick(entry.joystick);
    }
}

void JoystickManager::init() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetJoysticks(&count);
    if (!ids) return;
    for (int i = 0; i < count; ++i) {
        SDL_JoystickID id = ids[i];
        if (SDL_IsGamepad(id)) {
            SDL_Gamepad* gp = SDL_OpenGamepad(id);
            if (gp) {
                mGamepads.push_back({id, gp, nullptr});
                SDL_Log("[JoystickManager] Opened gamepad id=%u: %s", id, SDL_GetGamepadName(gp));
            }
        } else {
            SDL_Joystick* js = SDL_OpenJoystick(id);
            if (js) {
                mGamepads.push_back({id, nullptr, js});
                SDL_Log("[JoystickManager] Opened raw joystick id=%u: %s", id, SDL_GetJoystickName(js));
            }
        }
    }
    SDL_free(ids);
}

void JoystickManager::handleEvent(const SDL_Event& evt) {
    if (evt.type == SDL_EVENT_GAMEPAD_ADDED) {
        SDL_Gamepad* gp = SDL_OpenGamepad(evt.gdevice.which);
        if (gp) {
            mGamepads.push_back({evt.gdevice.which, gp, nullptr});
        }
    } else if (evt.type == SDL_EVENT_GAMEPAD_REMOVED) {
        for (auto it = mGamepads.begin(); it != mGamepads.end(); ++it) {
            if (it->id == evt.gdevice.which && it->gamepad) {
                SDL_CloseGamepad(it->gamepad);
                mGamepads.erase(it);
                break;
            }
        }
    } else if (evt.type == SDL_EVENT_JOYSTICK_ADDED) {
        if (!SDL_IsGamepad(evt.jdevice.which)) {
            SDL_Joystick* js = SDL_OpenJoystick(evt.jdevice.which);
            if (js) {
                mGamepads.push_back({evt.jdevice.which, nullptr, js});
                SDL_Log("[JoystickManager] Hot-plugged raw joystick id=%u: %s", evt.jdevice.which, SDL_GetJoystickName(js));
            }
        }
    } else if (evt.type == SDL_EVENT_JOYSTICK_REMOVED) {
        for (auto it = mGamepads.begin(); it != mGamepads.end(); ++it) {
            if (it->id == evt.jdevice.which && it->joystick) {
                SDL_CloseJoystick(it->joystick);
                mGamepads.erase(it);
                break;
            }
        }
    }
}

float JoystickManager::getAxisValue(int joystickId, int axis) const {
    if (joystickId < 0 || joystickId >= static_cast<int>(mGamepads.size())) return 0.0f;
    const auto& entry = mGamepads[joystickId];
    if (entry.gamepad) {
        Sint16 val = SDL_GetGamepadAxis(entry.gamepad, static_cast<SDL_GamepadAxis>(axis));
        return static_cast<float>(val) / 32767.0f;
    }
    if (entry.joystick) {
        Sint16 val = SDL_GetJoystickAxis(entry.joystick, axis);
        return static_cast<float>(val) / 32767.0f;
    }
    return 0.0f;
}

bool JoystickManager::isButtonDown(int joystickId, int button) const {
    if (joystickId < 0 || joystickId >= static_cast<int>(mGamepads.size())) return false;
    const auto& entry = mGamepads[joystickId];
    if (entry.gamepad) {
        return SDL_GetGamepadButton(entry.gamepad, static_cast<SDL_GamepadButton>(button));
    }
    if (entry.joystick) {
        return SDL_GetJoystickButton(entry.joystick, button);
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
