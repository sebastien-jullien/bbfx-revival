#include "MouseManager.h"

namespace bbfx {

MouseManager::MouseManager() = default;

void MouseManager::update() {
    SDL_GetMouseState(&mX, &mY);
    SDL_GetRelativeMouseState(&mDX, &mDY);
}

float MouseManager::getX() const { return mX; }
float MouseManager::getY() const { return mY; }
float MouseManager::getDX() const { return mDX; }
float MouseManager::getDY() const { return mDY; }

bool MouseManager::isButtonDown(int button) const {
    Uint32 state = SDL_GetMouseState(nullptr, nullptr);
    return (state & SDL_BUTTON_MASK(button)) != 0;
}

} // namespace bbfx
