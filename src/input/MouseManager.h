#pragma once

#include "../bbfx.h"
#include <SDL3/SDL.h>

namespace bbfx {

class MouseManager {
public:
    MouseManager();
    void update();

    float getX() const;
    float getY() const;
    float getDX() const;
    float getDY() const;
    bool isButtonDown(int button) const;

private:
    float mX = 0, mY = 0;
    float mDX = 0, mDY = 0;
};

} // namespace bbfx
