#pragma once

// v3.5 Lot J — `JoystickManager` was the v3.0..v3.4 name for this class.
// It is now an alias for the renamed-and-extended `GamepadManager` so that
// every existing `bbfx::JoystickManager` reference keeps compiling
// unchanged. New code should reach for GamepadManager directly.

#include "GamepadManager.h"

namespace bbfx {

using JoystickManager = GamepadManager;

} // namespace bbfx
