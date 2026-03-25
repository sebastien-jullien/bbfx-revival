-- input.lua — Lua wrapper for InputManager, KeyboardManager, MouseManager, JoystickManager
-- Compatible API with original bbfx input.keyboard:isKeyDown(...) pattern

local input = {}

input.keyboard = bbfx.InputManager.instance():getKeyboard()
input.mouse = bbfx.InputManager.instance():getMouse()
input.joystick = bbfx.InputManager.instance():getJoystick()

return input
