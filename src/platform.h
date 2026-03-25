#pragma once

// ── BBFx Platform Abstraction ───────────────────────────────────────────────
//
// Centralizes all platform-specific differences so that no #ifdef appears
// in application code (main.cpp, Engine, etc.).
//
// Platform values:
//   Windows (BBFX_WIN32):
//     - Renderer: "Direct3D11 Rendering Subsystem"
//     - Plugins:  RenderSystem_Direct3D11, Plugin_OctreeSceneManager,
//                 Plugin_ParticleFX
//
//   Linux (BBFX_LINUX):
//     - Renderer: "Vulkan Rendering Subsystem"
//     - Plugins:  RenderSystem_Vulkan, RenderSystem_GL3Plus,
//                 Plugin_OctreeSceneManager, Plugin_ParticleFX
//

#if defined(_WIN32)
  #define BBFX_WIN32
  #define BBFX_OGRE_RENDERER "Direct3D11 Rendering Subsystem"
  #define BBFX_OGRE_PLUGINS  {"RenderSystem_Direct3D11", \
                               "Plugin_OctreeSceneManager", \
                               "Plugin_ParticleFX"}
#else
  #define BBFX_LINUX
  #define BBFX_OGRE_RENDERER "Vulkan Rendering Subsystem"
  #define BBFX_OGRE_PLUGINS  {"RenderSystem_Vulkan", \
                               "RenderSystem_GL3Plus", \
                               "Plugin_OctreeSceneManager", \
                               "Plugin_ParticleFX"}
#endif

// ── Native window handle retrieval ──────────────────────────────────────────
// Encapsulated here so main.cpp has zero #ifdef.

#ifdef BBFX_WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#else
  #include <X11/Xlib.h>
#endif

#include <SDL3/SDL.h>
#include <string>
#include <stdexcept>
#include <cstdint>

inline std::string getNativeWindowHandle(SDL_Window* win) {
    SDL_PropertiesID props = SDL_GetWindowProperties(win);
#ifdef BBFX_WIN32
    HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(props,
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!hwnd) {
        throw std::runtime_error("Failed to get Win32 HWND from SDL3 window");
    }
    return std::to_string(reinterpret_cast<uintptr_t>(hwnd));
#else
    auto xwin = static_cast<::Window>(SDL_GetNumberProperty(props,
        SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
    if (xwin == 0) {
        throw std::runtime_error("Failed to get X11 Window from SDL3 window");
    }
    return std::to_string(xwin);
#endif
}
