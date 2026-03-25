#include "KeyboardManager.h"

namespace bbfx {

KeyboardManager::KeyboardManager() {
    mKeyState = SDL_GetKeyboardState(&mNumKeys);
}

void KeyboardManager::update() {
    mJustPressed.clear();
}

bool KeyboardManager::isKeyDown(SDL_Keycode key) const {
    SDL_Scancode sc = SDL_GetScancodeFromKey(key, nullptr);
    if (sc < mNumKeys) {
        return mKeyState[sc];
    }
    return false;
}

bool KeyboardManager::wasKeyPressed(SDL_Keycode key) const {
    auto it = mJustPressed.find(key);
    return it != mJustPressed.end() && it->second;
}

void KeyboardManager::handleEvent(const SDL_Event& evt) {
    if (evt.type == SDL_EVENT_KEY_DOWN && !evt.key.repeat) {
        mJustPressed[evt.key.key] = true;
    }
}

} // namespace bbfx
