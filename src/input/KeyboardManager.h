#pragma once

#include "../bbfx.h"
#include <SDL3/SDL.h>

namespace bbfx {

class KeyboardManager {
public:
    KeyboardManager();
    void update();

    bool isKeyDown(SDL_Keycode key) const;
    bool wasKeyPressed(SDL_Keycode key) const;

    void handleEvent(const SDL_Event& evt);

private:
    const bool* mKeyState = nullptr;
    int mNumKeys = 0;
    std::map<SDL_Keycode, bool> mJustPressed;
};

} // namespace bbfx
